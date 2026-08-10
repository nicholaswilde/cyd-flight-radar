#include <unity.h>
#include <ArduinoJson.h>
#include "utils_math.h"
#include "utils_adsb.h"
#include <cstring>

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

void test_is_military(void) {
    JsonDocument doc;
    doc["dbFlags"] = true;
    TEST_ASSERT_TRUE(utils::adsb::isMilitary(doc.as<JsonObject>()));
    doc["dbFlags"] = false;
    TEST_ASSERT_FALSE(utils::adsb::isMilitary(doc.as<JsonObject>()));
}

void test_pick_nose_heading(void) {
    JsonDocument doc;
    doc["true_heading"] = 120.5f;
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 120.5f, utils::adsb::pickNoseHeading(doc.as<JsonObject>()));
    doc.clear();
    doc["mag_heading"] = 90.0f;
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 90.0f, utils::adsb::pickNoseHeading(doc.as<JsonObject>()));
    doc.clear();
    doc["track"] = 180.0f;
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 180.0f, utils::adsb::pickNoseHeading(doc.as<JsonObject>()));
    doc.clear();
    doc["dir"] = 270.0f;
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 270.0f, utils::adsb::pickNoseHeading(doc.as<JsonObject>()));
    doc.clear();
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, utils::adsb::pickNoseHeading(doc.as<JsonObject>()));
}

void test_pick_track_heading(void) {
    JsonDocument doc;
    doc["track"] = 45.0f;
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 45.0f, utils::adsb::pickTrackHeading(doc.as<JsonObject>()));
    doc.clear();
    doc["true_heading"] = 120.5f;
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 120.5f, utils::adsb::pickTrackHeading(doc.as<JsonObject>()));
    doc.clear();
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, utils::adsb::pickTrackHeading(doc.as<JsonObject>()));
}

void test_pick_ground_speed(void) {
    JsonDocument doc;
    doc["gs"] = 450.5f;
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 450.5f, utils::adsb::pickGroundSpeed(doc.as<JsonObject>()));
    doc.clear();
    doc["tas"] = 400.0f;
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 400.0f, utils::adsb::pickGroundSpeed(doc.as<JsonObject>()));
    doc.clear();
    doc["ias"] = 250.0f;
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 250.0f, utils::adsb::pickGroundSpeed(doc.as<JsonObject>()));
    doc.clear();
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, utils::adsb::pickGroundSpeed(doc.as<JsonObject>()));
}

void test_is_on_ground(void) {
    JsonDocument doc;
    doc["alt_baro"] = "ground";
    TEST_ASSERT_TRUE(utils::adsb::isOnGround(doc.as<JsonObject>()));
    doc["alt_baro"] = 35000;
    TEST_ASSERT_FALSE(utils::adsb::isOnGround(doc.as<JsonObject>()));
}

void test_copy_json_string_trimmed(void) {
    JsonDocument doc;
    char buffer[32];
    
    // Normal string
    doc["hex"] = "a1b2c3";
    utils::adsb::copyJsonStringTrimmed(doc.as<JsonObject>(), "hex", buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL_STRING("a1b2c3", buffer);
    
    // String with trailing spaces
    doc["flight"] = "AAL123  ";
    utils::adsb::copyJsonStringTrimmed(doc.as<JsonObject>(), "flight", buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL_STRING("AAL123", buffer);
    
    // Missing key
    utils::adsb::copyJsonStringTrimmed(doc.as<JsonObject>(), "missing", buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL_STRING("", buffer);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_km_to_nm);
    RUN_TEST(test_km_to_miles);
    RUN_TEST(test_miles_to_km);
    RUN_TEST(test_is_military);
    RUN_TEST(test_pick_nose_heading);
    RUN_TEST(test_pick_track_heading);
    RUN_TEST(test_pick_ground_speed);
    RUN_TEST(test_is_on_ground);
    RUN_TEST(test_copy_json_string_trimmed);
    return UNITY_END();
}
