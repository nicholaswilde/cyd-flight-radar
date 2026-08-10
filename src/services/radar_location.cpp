#include "services/radar_location.h"

#include <Preferences.h>
#include <cstdlib>
#include <cstring>

#include "config.h"
#include "utils_math.h"

namespace services::location {

namespace {

constexpr char kPrefsNamespace[] = "radar";
constexpr char kKeyLat[] = "lat";
constexpr char kKeyLon[] = "lon";

double s_lat = config::kDefaultRadarLat;
double s_lon = config::kDefaultRadarLon;



void persist(double lat, double lon) {
  Preferences prefs;
  prefs.begin(kPrefsNamespace, false);
  prefs.putDouble(kKeyLat, lat);
  prefs.putDouble(kKeyLon, lon);
  prefs.end();
  s_lat = lat;
  s_lon = lon;
}

}  // namespace

void init() {
  Preferences prefs;
  prefs.begin(kPrefsNamespace, true);
  if (prefs.isKey(kKeyLat) && prefs.isKey(kKeyLon)) {
    const double lat = prefs.getDouble(kKeyLat, config::kDefaultRadarLat);
    const double lon = prefs.getDouble(kKeyLon, config::kDefaultRadarLon);
    if (utils::math::validLatLon(lat, lon)) {
      s_lat = lat;
      s_lon = lon;
    }
  }
  prefs.end();
}

double lat() { return s_lat; }

double lon() { return s_lon; }

bool saveFromStrings(const char* lat_str, const char* lon_str) {
  double lat = 0.0;
  double lon = 0.0;
  if (!utils::math::parseCoord(lat_str, &lat) || !utils::math::parseCoord(lon_str, &lon)) {
    return false;
  }
  if (!utils::math::validLatLon(lat, lon)) {
    return false;
  }
  persist(lat, lon);
  Serial.printf("Radar location saved: %.6f, %.6f\n", lat, lon);
  return true;
}

void clear() {
  Preferences prefs;
  prefs.begin(kPrefsNamespace, false);
  prefs.remove(kKeyLat);
  prefs.remove(kKeyLon);
  prefs.end();
  s_lat = config::kDefaultRadarLat;
  s_lon = config::kDefaultRadarLon;
}

}  // namespace services::location
