#include <unity.h>
#include "utils_math.h"

void setUp(void) {}
void tearDown(void) {}

void test_km_to_nm(void) {
    float result = utils::math::kmToNauticalMiles(1.852f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, result);
}

void test_km_to_miles(void) {
    float result = utils::math::kmToMiles(1.609344f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, result);
}

void test_miles_to_km(void) {
    float result = utils::math::milesToKm(1.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.609344f, result);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_km_to_nm);
    RUN_TEST(test_km_to_miles);
    RUN_TEST(test_miles_to_km);
    return UNITY_END();
}
