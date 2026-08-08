#include "services/adsb_client.h"

#include <HTTPClient.h>
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
constexpr float kKmPerNm = 1.852f;
constexpr int kConnectAttemptMs = 4000;
constexpr unsigned long kRequestTimeoutMs = 10000;

Aircraft s_aircraft[kMaxAircraft];
Aircraft s_aircraft_buffer[kMaxAircraft];
size_t s_aircraft_count = 0;

static char s_route_callsign[9] = {0};
static char s_route_hex[7] = {0};

WiFiClientSecure s_client;
HTTPClient s_http;



int performGetWithPoll(HTTPClient& http) {
  http.setConnectTimeout(kConnectAttemptMs);
  const unsigned long deadline = millis() + kRequestTimeoutMs;
  while (millis() < deadline) {
    const int code = http.GET();
    if (code > 0) {
      return code;
    }
    if (code != HTTPC_ERROR_CONNECTION_REFUSED &&
        code != HTTPC_ERROR_NOT_CONNECTED) {
      return code;
    }
    delay(500);
  }
  return HTTPC_ERROR_READ_TIMEOUT;
}

float kmToNauticalMiles(float km) { return km / kKmPerNm; }

bool readJsonFloat(const JsonObject& obj, const char* key, float* out) {
  if (obj[key].is<float>() || obj[key].is<double>() || obj[key].is<int>()) {
    *out = obj[key].as<float>();
    return true;
  }
  return false;
}

bool isMilitary(const JsonObject& plane) {
  return plane["dbFlags"].is<bool>() && plane["dbFlags"].as<bool>();
}

float pickNoseHeading(const JsonObject& plane) {
  float v = 0.0f;
  if (readJsonFloat(plane, "true_heading", &v)) {
    return v;
  }
  if (readJsonFloat(plane, "mag_heading", &v)) {
    return v;
  }
  if (readJsonFloat(plane, "track", &v)) {
    return v;
  }
  if (readJsonFloat(plane, "dir", &v)) {
    return v;
  }
  return 0.0f;
}

float pickTrackHeading(const JsonObject& plane) {
  float v = 0.0f;
  if (readJsonFloat(plane, "track", &v)) {
    return v;
  }
  if (readJsonFloat(plane, "true_heading", &v)) {
    return v;
  }
  if (readJsonFloat(plane, "mag_heading", &v)) {
    return v;
  }
  if (readJsonFloat(plane, "dir", &v)) {
    return v;
  }
  return 0.0f;
}

float pickGroundSpeed(const JsonObject& plane) {
  float v = 0.0f;
  if (readJsonFloat(plane, "gs", &v)) {
    return v;
  }
  if (readJsonFloat(plane, "tas", &v)) {
    return v;
  }
  if (readJsonFloat(plane, "ias", &v)) {
    return v;
  }
  return 0.0f;
}

bool isOnGround(const JsonObject& plane) {
  if (!plane["alt_baro"].is<const char*>()) {
    return false;
  }
  return strcmp(plane["alt_baro"].as<const char*>(), "ground") == 0;
}

void copyJsonStringTrimmed(const JsonObject& obj, const char* key, char* out,
                           size_t out_len) {
  out[0] = '\0';
  if (out_len == 0 || !obj[key].is<const char*>()) {
    return;
  }
  const char* s = obj[key].as<const char*>();
  size_t n = strnlen(s, out_len - 1);
  while (n > 0 && s[n - 1] == ' ') {
    --n;
  }
  memcpy(out, s, n);
  out[n] = '\0';
}

bool isHelicopter(const JsonObject& plane) {
  if (plane["category"].is<const char*>()) {
    const char* cat = plane["category"].as<const char*>();
    if (strcmp(cat, "A7") == 0) {
      return true;
    }
  }
  if (plane["desc"].is<const char*>()) {
    const char* desc = plane["desc"].as<const char*>();
    if (strstr(desc, "HELICOPTER") != nullptr || strstr(desc, "ROTORCRAFT") != nullptr || strstr(desc, "Helicopter") != nullptr || strstr(desc, "Rotorcraft") != nullptr) {
      return true;
    }
  }
  return false;
}

void formatAltitudeTag(const JsonObject& plane, char* out, size_t out_len) {
  out[0] = '\0';
  if (out_len == 0) {
    return;
  }

  if (plane["alt_baro"].is<const char*>()) {
    const char* s = plane["alt_baro"].as<const char*>();
    if (strcmp(s, "ground") == 0) {
      strncpy(out, "GND", out_len - 1);
      out[out_len - 1] = '\0';
      return;
    }
  }

  float alt = 0.0f;
  if (readJsonFloat(plane, "alt_baro", &alt) ||
      readJsonFloat(plane, "alt_geom", &alt)) {
    if (!ui::radar::useMiles()) {
      snprintf(out, out_len, "%d m", static_cast<int>(lroundf(alt * 0.3048f)));
    } else {
      snprintf(out, out_len, "%d ft", static_cast<int>(lroundf(alt)));
    }
    return;
  }
}

void fillTagFields(Aircraft* ac, const JsonObject& plane) {
  copyJsonStringTrimmed(plane, "hex", ac->hex, sizeof(ac->hex));
  copyJsonStringTrimmed(plane, "flight", ac->callsign, sizeof(ac->callsign));
  if (ac->callsign[0] == '\0') {
    copyJsonStringTrimmed(plane, "hex", ac->callsign, sizeof(ac->callsign));
  }

  copyJsonStringTrimmed(plane, "t", ac->type, sizeof(ac->type));
  formatAltitudeTag(plane, ac->alt, sizeof(ac->alt));
  
  copyJsonStringTrimmed(plane, "r", ac->reg, sizeof(ac->reg));
  copyJsonStringTrimmed(plane, "desc", ac->desc, sizeof(ac->desc));
}

}  // namespace

size_t aircraftCount() { return s_aircraft_count; }

const Aircraft* aircraftList() { return s_aircraft; }

void ensureClientConfigured() {
  s_client.setInsecure();
  s_http.setReuse(false);
}

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
    // Find start of object or end of array
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
  Serial.printf("fetch: free=%u largest=%u\n",
      ESP.getFreeHeap(),
      heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));

  // Pre-allocate static filters BEFORE SSL starts so they don't fragment the heap
  filterDoc();
  static JsonDocument routeFilter;
  if (routeFilter.isNull()) {
    routeFilter["response"]["flightroute"]["origin"]["iata_code"] = true;
    routeFilter["response"]["flightroute"]["origin"]["icao_code"] = true;
    routeFilter["response"]["flightroute"]["destination"]["iata_code"] = true;
    routeFilter["response"]["flightroute"]["destination"]["icao_code"] = true;
  }

  if (s_route_callsign[0] != '\0' && s_route_callsign[0] != '~') {
    char callsign[9];
    char hex[7];
    strncpy(callsign, s_route_callsign, sizeof(callsign) - 1);
    callsign[sizeof(callsign) - 1] = '\0';
    strncpy(hex, s_route_hex, sizeof(hex) - 1);
    hex[sizeof(hex) - 1] = '\0';
    s_route_callsign[0] = '\0';
    s_route_hex[0] = '\0';

    Aircraft* target = nullptr;
    for (size_t i = 0; i < s_aircraft_count; ++i) {
      if (strcmp(s_aircraft[i].hex, hex) == 0) {
        target = &s_aircraft[i];
        break;
      }
    }

    if (target && target->route_origin[0] == '\0') {
      ensureClientConfigured();
      String url = String("https://api.adsbdb.com/v0/callsign/") + callsign;
      Serial.printf("route: fetching %s\n", url.c_str());
      if (s_http.begin(s_client, url)) {
        s_http.setTimeout(kRequestTimeoutMs);
        int code = s_http.GET();
        Serial.printf("route: HTTP %d\n", code);
        if (code == HTTP_CODE_OK) {
          WiFiClient* stream = s_http.getStreamPtr();
          if (stream != nullptr) {
            JsonDocument doc;
            doc.clear();
            DeserializationError err = deserializeJson(doc, *stream, DeserializationOption::Filter(routeFilter));
            if (err) {
              Serial.printf("route: parse error: %s\n", err.c_str());
            } else {
              JsonObject originObj = doc["response"]["flightroute"]["origin"];
              JsonObject destObj = doc["response"]["flightroute"]["destination"];

              const char* origin = nullptr;
              if (originObj["iata_code"].is<const char*>()) {
                origin = originObj["iata_code"].as<const char*>();
              } else if (originObj["icao_code"].is<const char*>()) {
                origin = originObj["icao_code"].as<const char*>();
              }

              const char* dest = nullptr;
              if (destObj["iata_code"].is<const char*>()) {
                dest = destObj["iata_code"].as<const char*>();
              } else if (destObj["icao_code"].is<const char*>()) {
                dest = destObj["icao_code"].as<const char*>();
              }

              Serial.printf("route: origin=%s dest=%s\n",
                  origin ? origin : "(null)", dest ? dest : "(null)");

              if (origin != nullptr) {
                strncpy(target->route_origin, origin, sizeof(target->route_origin) - 1);
                target->route_origin[sizeof(target->route_origin) - 1] = '\0';
              }
              if (dest != nullptr) {
                strncpy(target->route_destination, dest, sizeof(target->route_destination) - 1);
                target->route_destination[sizeof(target->route_destination) - 1] = '\0';
              }
            }
          }
        }
        s_http.end();
        s_client.stop();
      }
    }
    return true;
  }

  const float dist_nm = kmToNauticalMiles(fetch_radius_km);

  String url = kApiBase;
  url += String(center_lat, 6);
  url += "/lon/";
  url += String(center_lon, 6);
  url += "/dist/";
  url += String(dist_nm, 1);

  ensureClientConfigured();

  if (!s_http.begin(s_client, url)) {
    Serial.println("adsb: http.begin failed");
    return false;
  }

  s_http.setTimeout(kRequestTimeoutMs);
  const int code = performGetWithPoll(s_http);
  if (code != HTTP_CODE_OK) {
    Serial.printf("adsb: HTTP %d\n", code);
    s_http.end();
    s_client.stop();
    return false;
  }

  WiFiClient* client_stream = s_http.getStreamPtr();
  if (client_stream == nullptr) {
    Serial.println("adsb: no stream available");
    s_http.end();
    s_client.stop();
    return false;
  }

  class ChunkedStream : public Stream {
   public:
    explicit ChunkedStream(Stream* inner) : inner_(inner) {}
  
    int available() override {
      if (eof_) return 0;
      if (chunk_left_ == 0 && !readChunkHeader()) return 0;
      int avail = inner_->available();
      return avail < chunk_left_ ? avail : chunk_left_;
    }
  
    int read() override {
      if (eof_) return -1;
      if (chunk_left_ == 0 && !readChunkHeader()) return -1;
      
      long timeout = millis() + 10000;
      while (inner_->available() == 0) {
        if (millis() > timeout) return -1;
        delay(1);
      }
      
      int c = inner_->read();
      if (c >= 0) {
        chunk_left_--;
        if (chunk_left_ == 0) {
          long to = millis() + 5000;
          while (inner_->available() < 2 && millis() < to) delay(1);
          inner_->read();
          inner_->read();
        }
      }
      return c;
    }
  
    int peek() override {
      if (eof_) return -1;
      if (chunk_left_ == 0 && !readChunkHeader()) return -1;
      long timeout = millis() + 10000;
      while (inner_->available() == 0) {
        if (millis() > timeout) return -1;
        delay(1);
      }
      return inner_->peek();
    }
    
    size_t write(uint8_t) override { return 0; }
  
   private:
    bool readChunkHeader() {
      if (eof_) return false;
      String header;
      int retries = 5;
      do {
        header = inner_->readStringUntil('\n');
        header.trim();
        if (header.length() == 0) {
          retries--;
          if (retries <= 0) return false;
        }
      } while (header.length() == 0);
      chunk_left_ = strtol(header.c_str(), nullptr, 16);
      if (chunk_left_ == 0) {
        eof_ = true;
        return false;
      }
      return true;
    }
  
    Stream* inner_;
    long chunk_left_ = 0;
    bool eof_ = false;
  };

  ChunkedStream chunked(client_stream);
  Stream& stream = (s_http.getSize() == -1) ? static_cast<Stream&>(chunked) : static_cast<Stream&>(*client_stream);

  // Search for '"ac":[ using safeStreamRead so delay(1) keeps IDLE0 fed
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
      s_http.end();
      s_client.stop();
      return false;
    }
  }

  size_t n = 0;
  static char jsonStr[2048];
  JsonDocument doc;
  while (extractNextJsonObject(stream, jsonStr, sizeof(jsonStr))) {
    doc.clear();
    DeserializationError err = deserializeJson(doc, jsonStr, DeserializationOption::Filter(filterDoc()));
    if (err) {
      Serial.printf("adsb: plane JSON parse error: %s\n", err.c_str());
      continue;
    }
    JsonObject plane = doc.as<JsonObject>();
    if (!plane["lat"].is<float>() || !plane["lon"].is<float>()) {
      continue;
    }
    if (isOnGround(plane)) {
      if (!ui::settings::isGroundAircraftEnabled()) continue;
    } else {
      float alt = 0.0f;
      if (readJsonFloat(plane, "alt_baro", &alt) || readJsonFloat(plane, "alt_geom", &alt)) {
        if (alt > ui::settings::getMaxAltitudeFilter()) {
          continue;
        }
      }
    }

    float p_lat = plane["lat"].as<float>();
    float p_lon = plane["lon"].as<float>();

    size_t aircraft_limit = ui::settings::getMaxAircraftLimit();
    if (aircraft_limit > kMaxAircraft) aircraft_limit = kMaxAircraft;

    Aircraft* target_ac = nullptr;
    if (n >= aircraft_limit) {
      float furthest_dist = -1.0f;
      size_t furthest_idx = 0;
      float cos_lat = cos(center_lat * M_PI / 180.0f);
      for (size_t i = 0; i < aircraft_limit; ++i) {
        float dLat = s_aircraft_buffer[i].lat - center_lat;
        float dLon = (s_aircraft_buffer[i].lon - center_lon) * cos_lat;
        float d = dLat * dLat + dLon * dLon;
        if (d > furthest_dist) {
          furthest_dist = d;
          furthest_idx = i;
        }
      }
      
      float dLat = p_lat - center_lat;
      float dLon = (p_lon - center_lon) * cos_lat;
      float new_dist = dLat * dLat + dLon * dLon;
      
      if (new_dist < furthest_dist) {
        target_ac = &s_aircraft_buffer[furthest_idx];
      } else {
        continue;
      }
    } else {
      target_ac = &s_aircraft_buffer[n];
      ++n;
    }

    target_ac->lat = p_lat;
    target_ac->lon = p_lon;
    target_ac->nose_deg = pickNoseHeading(plane);
    target_ac->track_deg = pickTrackHeading(plane);
    target_ac->gs_knots = pickGroundSpeed(plane);
    target_ac->is_heli = isHelicopter(plane);
    target_ac->is_military = isMilitary(plane);
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

  s_http.end();
  s_client.stop();

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
