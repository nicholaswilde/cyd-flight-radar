#include "utils_math.h"

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

} // namespace math
} // namespace utils
