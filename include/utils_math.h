#pragma once

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

} // namespace math
} // namespace utils
