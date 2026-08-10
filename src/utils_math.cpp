#include "utils_math.h"
#include <cstdlib>
#include <cstdio>
#include <cmath>

namespace utils {
namespace math {

float kmToNauticalMiles(float km) {
    return km / kKmPerNm;
}

float kmToMiles(float km) {
    return km / kKmPerMile;
}

float milesToKm(float miles) {
    return miles * kKmPerMile;
}

bool parseCoord(const char* text, double* out) {
  if (text == nullptr || text[0] == '\0') {
    return false;
  }
  char* end = nullptr;
  const double v = std::strtod(text, &end);
  if (end == text || (end != nullptr && *end != '\0')) {
    return false;
  }
  *out = v;
  return true;
}

bool validLatLon(double lat, double lon) {
  return lat >= -90.0 && lat <= 90.0 && lon >= -180.0 && lon <= 180.0;
}

void formatRing3Label(char* buf, size_t len, float ring3_km, bool use_miles) {
  if (use_miles) {
    const int mi = static_cast<int>(std::lround(ring3_km / kKmPerMile));
    std::snprintf(buf, len, "%dmi", mi);
  } else {
    const int km = static_cast<int>(std::lround(ring3_km));
    std::snprintf(buf, len, "%dkm", km);
  }
}

} // namespace math
} // namespace utils
