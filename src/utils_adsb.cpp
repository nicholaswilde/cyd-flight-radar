#include "utils_adsb.h"
#include <cstring>
#include <cmath>
#include <cstdio>

namespace utils {
namespace adsb {

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
  if (readJsonFloat(plane, "true_heading", &v)) return v;
  if (readJsonFloat(plane, "mag_heading", &v)) return v;
  if (readJsonFloat(plane, "track", &v)) return v;
  if (readJsonFloat(plane, "dir", &v)) return v;
  return 0.0f;
}

float pickTrackHeading(const JsonObject& plane) {
  float v = 0.0f;
  if (readJsonFloat(plane, "track", &v)) return v;
  if (readJsonFloat(plane, "true_heading", &v)) return v;
  if (readJsonFloat(plane, "mag_heading", &v)) return v;
  if (readJsonFloat(plane, "dir", &v)) return v;
  return 0.0f;
}

float pickGroundSpeed(const JsonObject& plane) {
  float v = 0.0f;
  if (readJsonFloat(plane, "gs", &v)) return v;
  if (readJsonFloat(plane, "tas", &v)) return v;
  if (readJsonFloat(plane, "ias", &v)) return v;
  return 0.0f;
}

bool isOnGround(const JsonObject& plane) {
  if (!plane["alt_baro"].is<const char*>()) {
    return false;
  }
  return strcmp(plane["alt_baro"].as<const char*>(), "ground") == 0;
}

void copyJsonStringTrimmed(const JsonObject& obj, const char* key, char* out, size_t out_len) {
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

void formatAltitudeTag(const JsonObject& plane, char* out, size_t out_len, bool useMiles) {
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
    if (!useMiles) {
      snprintf(out, out_len, "%d m", static_cast<int>(lroundf(alt * 0.3048f)));
    } else {
      snprintf(out, out_len, "%d ft", static_cast<int>(lroundf(alt)));
    }
    return;
  }
}

} // namespace adsb
} // namespace utils
