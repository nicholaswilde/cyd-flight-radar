#pragma once
#include <cstdint>

namespace utils {
namespace math {

constexpr float kKmPerNm = 1.852f;
constexpr float kKmPerMile = 1.609344f;

float kmToNauticalMiles(float km);
float kmToMiles(float km);
float milesToKm(float miles);

bool parseCoord(const char* text, double* out);
bool validLatLon(double lat, double lon);
void formatRing3Label(char* buf, decltype(sizeof(0)) len, float ring3_km, bool use_miles);

void equirectangularOffsetKm(float center_lat, float center_lon, float lat, float lon, float* dx_km, float* dy_km, float* dist_km);
float e7ToDeg(int32_t e7);

} // namespace math
} // namespace utils
