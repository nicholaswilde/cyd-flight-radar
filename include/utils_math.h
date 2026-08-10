#pragma once

namespace utils {
namespace math {

constexpr float kKmPerNm = 1.852f;
constexpr float kKmPerMile = 1.609344f;

float kmToNauticalMiles(float km);
float kmToMiles(float km);
float milesToKm(float miles);

} // namespace math
} // namespace utils
