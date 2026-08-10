#include "utils_math.h"
#include "utils_adsb.h"
#include "services/adsb_client.h"

#include <WiFiClientSecure.h>

#include <ArduinoJson.h>

#include <cstring>

#include "config.h"
#include "ui/settings_menu.h"
#include "ui/radar_range.h"
#include "services/radar_location.h"

namespace services::adsb {

namespace {

constexpr char kApiBase[] = "https://opendata.adsb.fi/api/v3/lat/";
// constexpr float kKmPerNm = 1.852f;
constexpr int kConnectAttemptMs = 4000;
constexpr unsigned long kRequestTimeoutMs = 10000;

Aircraft s_aircraft[kMaxAircraft];
Aircraft s_aircraft_buffer[kMaxAircraft];
size_t s_aircraft_count = 0;

// WiFiClientSecure is a module-scope singleton intentionally.
// Its constructor allocates ~6KB of persistent state (sslclient_context).
// By defining it here and pre-warming it in init() BEFORE WiFi starts, this 6KB
// is placed at the bottom of the heap. 
WiFiClientSecure s_client;

static char s_route_callsign[9] = {0};
static char s_route_hex[7] = {0};





void fillTagFields(Aircraft* ac, const JsonObject& plane) {
  utils::adsb::copyJsonStringTrimmed(plane, "hex", ac->hex, sizeof(ac->hex));
  utils::adsb::copyJsonStringTrimmed(plane, "flight", ac->callsign, sizeof(ac->callsign));
  if (ac->callsign[0] == '\0') {
    utils::adsb::copyJsonStringTrimmed(plane, "hex", ac->callsign, sizeof(ac->callsign));
  }

  utils::adsb::copyJsonStringTrimmed(plane, "t", ac->type, sizeof(ac->type));
  utils::adsb::formatAltitudeTag(plane, ac->alt, sizeof(ac->alt), ui::radar::useMiles());
  
  utils::adsb::copyJsonStringTrimmed(plane, "r", ac->reg, sizeof(ac->reg));
  utils::adsb::copyJsonStringTrimmed(plane, "desc", ac->desc, sizeof(ac->desc));
}

}  // namespace

size_t aircraftCount() { return s_aircraft_count; }

const Aircraft* aircraftList() { return s_aircraft; }

JsonDocument& filterDoc() {
  static JsonDocument filter;
  static bool initialized = false;
  if (!initialized) {
    filter["hex"] = true;
    filter["flight"] = true;
    filter["t"] = true;
    filter["lat"] = true;
    filter["lon"] = true;
    filter["alt_baro"] = true;
    filter["alt_geom"] = true;
    filter["gs"] = true;
    filter["tas"] = true;
    filter["ias"] = true;
    filter["track"] = true;
    filter["true_heading"] = true;
    filter["mag_heading"] = true;
    filter["dir"] = true;
    filter["dbFlags"] = true;
    filter["category"] = true;
    filter["desc"] = true;
    filter["r"] = true;
    initialized = true;
  }
  return filter;
}

void init() {
  filterDoc();
  s_client.setInsecure();
}

static int safeStreamRead(Stream& stream) {
    long timeout = millis() + 5000;
    while (stream.available() == 0) {
        if (millis() > timeout) return -1;
        delay(1);
    }
    return stream.read();
}

static bool extractNextJsonObject(Stream& stream, char* buffer, size_t max_len) {
    size_t len = 0;
    int c;
    while ((c = safeStreamRead(stream)) != '{') {
        if (c == -1) return false;
        if (c == ']') return false;
    }
    int braceCount = 1;
    if (len < max_len - 1) buffer[len++] = '{';
    bool inString = false;
    bool escape = false;
    while (braceCount > 0) {
        c = safeStreamRead(stream);
        if (c == -1) return false;
        if (len < max_len - 1) buffer[len++] = (char)c;
        if (escape) {
            escape = false;
        } else if (c == '\\') {
            escape = true;
        } else if (c == '"') {
            inString = !inString;
        } else if (!inString) {
            if (c == '{') braceCount++;
            else if (c == '}') braceCount--;
        }
    }
    buffer[len] = '\0';
    return true;
}

bool fetchUpdate(double center_lat, double center_lon, float fetch_radius_km) {
#define HEAP() Serial.printf("heap[%s]: free=%u largest=%u\n", __func__, \
    ESP.getFreeHeap(), heap_caps_get_largest_free_block(MALLOC_CAP_8BIT))
  HEAP(); // [0] baseline before anything

  filterDoc();
  s_client.setInsecure();

  if (s_route_callsign[0] != '\0') {
    s_route_callsign[0] = '\0';
    s_route_hex[0] = '\0';
  }

  const float dist_nm = utils::math::kmToNauticalMiles(fetch_radius_km);

  int retries = 0;
  bool connected = false;
  while (retries < 3 && !connected) {
    if (s_client.connect("opendata.adsb.fi", 443)) {
      connected = true;
    } else {
      s_client.stop(); // Prevent leaks on failed attempts
      retries++;
      delay(500);
    }
  }

  if (!connected) {
    Serial.println("adsb: connect failed");
    return false;
  }

  s_client.printf("GET /api/v3/lat/%.6f/lon/%.6f/dist/%.1f HTTP/1.0\r\n"
                  "Host: opendata.adsb.fi\r\n"
                  "Connection: close\r\n"
                  "User-Agent: CYD-Flight-Radar/1.0\r\n"
                  "\r\n", center_lat, center_lon, dist_nm);

  const char endOfHeaders[] = "\r\n\r\n";
  int hdrMatch = 0;
  bool headersPassed = false;
  unsigned long hdrTimeout = millis() + 5000;
  
  while (s_client.connected() || s_client.available()) {
    if (millis() > hdrTimeout) break;
    if (s_client.available()) {
      int c = s_client.read();
      if (c == endOfHeaders[hdrMatch]) {
        hdrMatch++;
        if (hdrMatch == 4) {
          headersPassed = true;
          break;
        }
      } else {
        hdrMatch = (c == '\r') ? 1 : 0;
      }
    } else {
      delay(1);
    }
  }

  if (!headersPassed) {
    Serial.println("adsb: failed to parse headers");
    s_client.stop();
    return false;
  }

  Stream& stream = s_client;

  {
    constexpr char kTarget[] = "\"ac\":[";
    constexpr int kTargetLen = 6;
    int match = 0;
    bool found = false;
    const unsigned long deadline = millis() + 10000;
    while (millis() < deadline) {
      int c = safeStreamRead(stream);
      if (c == -1) break;
      if ((char)c == kTarget[match]) {
        if (++match == kTargetLen) { found = true; break; }
      } else {
        match = ((char)c == kTarget[0]) ? 1 : 0;
      }
    }
    if (!found) {
      Serial.println("adsb: JSON parse error: missing ac array");
      s_client.stop();
      return false;
    }
  }

  size_t n = 0;
  static char jsonStr[2048];

  { // Block scope
  while (extractNextJsonObject(stream, jsonStr, sizeof(jsonStr))) {
    JsonDocument doc; // Local to iteration to prevent heap interleaving with LwIP pbufs
    DeserializationError err = deserializeJson(doc, jsonStr, DeserializationOption::Filter(filterDoc()));
    if (err) {
      Serial.printf("adsb: plane JSON parse error: %s\n", err.c_str());
      continue;
    }
    JsonObject plane = doc.as<JsonObject>();
    if (!plane["lat"].is<float>() || !plane["lon"].is<float>()) {
      continue;
    }
    if (utils::adsb::isOnGround(plane)) {
      if (!ui::settings::isGroundAircraftEnabled()) continue;
    } else {
      float alt = 0.0f;
      if (utils::adsb::readJsonFloat(plane, "alt_baro", &alt) || utils::adsb::readJsonFloat(plane, "alt_geom", &alt)) {
        if (alt > ui::settings::getMaxAltitudeFilter()) {
          continue;
        }
      }
    }

    float p_lat = plane["lat"].as<float>();
    float p_lon = plane["lon"].as<float>();

    size_t aircraft_limit = ui::settings::getMaxAircraftLimit();
    if (aircraft_limit > kMaxAircraft) aircraft_limit = kMaxAircraft;

    // NOTE: Closest-aircraft replacement logic removed — it ran a full O(n)
    // distance loop on every incoming plane once the buffer was full, burning
    // CPU and stack during JSON parsing. Aircraft are now simply capped in
    // arrival order (the API already returns them ordered by distance).
    if (n >= aircraft_limit) {
      continue;
    }
    Aircraft* target_ac = &s_aircraft_buffer[n];
    ++n;

    target_ac->lat = p_lat;
    target_ac->lon = p_lon;
    target_ac->nose_deg = utils::adsb::pickNoseHeading(plane);
    target_ac->track_deg = utils::adsb::pickTrackHeading(plane);
    target_ac->gs_knots = utils::adsb::pickGroundSpeed(plane);
    target_ac->is_heli = utils::adsb::isHelicopter(plane);
    target_ac->is_military = utils::adsb::isMilitary(plane);
    fillTagFields(target_ac, plane);
    
    target_ac->trail_size = 0;
    target_ac->trail_head = 0;
    target_ac->route_origin[0] = '\0';
    target_ac->route_destination[0] = '\0';
    if (target_ac->hex[0] != '\0') {
      for (size_t i = 0; i < s_aircraft_count; ++i) {
        if (strcmp(s_aircraft[i].hex, target_ac->hex) == 0) {
          target_ac->trail_size = s_aircraft[i].trail_size;
          target_ac->trail_head = s_aircraft[i].trail_head;
          memcpy(target_ac->trail, s_aircraft[i].trail, sizeof(target_ac->trail));
          strcpy(target_ac->route_origin, s_aircraft[i].route_origin);
          strcpy(target_ac->route_destination, s_aircraft[i].route_destination);
          break;
        }
      }
    }
    
    size_t last_idx = (target_ac->trail_head + 5) % 6;
    if (target_ac->trail_size == 0 || target_ac->trail[last_idx].lat != p_lat || target_ac->trail[last_idx].lon != p_lon) {
      target_ac->trail[target_ac->trail_head].lat = p_lat;
      target_ac->trail[target_ac->trail_head].lon = p_lon;
      target_ac->trail_head = (target_ac->trail_head + 1) % 6;
      if (target_ac->trail_size < 6) target_ac->trail_size++;
    }
  }
  
  // Double buffer copy
  memcpy(s_aircraft, s_aircraft_buffer, n * sizeof(Aircraft));
  s_aircraft_count = n;

  Serial.printf("adsb: %u aircraft\n", static_cast<unsigned>(n));

  } // end block scope: doc destructs HERE, freeing its pool BEFORE http.end()
    // + client.stop(). This ensures the heap allocator sees doc's freed region
    // and SSL's freed region simultaneously and can coalesce them (esp32-ssl-heap).
  HEAP(); // [5] after doc destructor (pool freed)
  s_client.stop();
  HEAP(); // [6] after client.stop (SSL freed)

#undef HEAP
  return true;
}

void requestRouteFetch(const char* callsign, const char* hex) {
  if (callsign == nullptr || hex == nullptr) return;
  strncpy(s_route_callsign, callsign, sizeof(s_route_callsign) - 1);
  s_route_callsign[sizeof(s_route_callsign) - 1] = '\0';
  strncpy(s_route_hex, hex, sizeof(s_route_hex) - 1);
  s_route_hex[sizeof(s_route_hex) - 1] = '\0';
}

}  // namespace services::adsb
