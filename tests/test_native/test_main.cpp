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


void test_is_helicopter(void) {
    JsonDocument doc;
    doc["category"] = "A7";
    TEST_ASSERT_TRUE(utils::adsb::isHelicopter(doc.as<JsonObject>()));
    doc["category"] = "A1";
    TEST_ASSERT_FALSE(utils::adsb::isHelicopter(doc.as<JsonObject>()));
    
    doc.clear();
    doc["desc"] = "Some HELICOPTER text";
    TEST_ASSERT_TRUE(utils::adsb::isHelicopter(doc.as<JsonObject>()));
    
    doc.clear();
    doc["desc"] = "A random plane";
    TEST_ASSERT_FALSE(utils::adsb::isHelicopter(doc.as<JsonObject>()));
}

void test_format_altitude_tag(void) {
    JsonDocument doc;
    char buffer[16];
    
    doc["alt_baro"] = "ground";
    utils::adsb::formatAltitudeTag(doc.as<JsonObject>(), buffer, sizeof(buffer), true);
    TEST_ASSERT_EQUAL_STRING("GND", buffer);
    
    doc.clear();
    doc["alt_baro"] = 35000;
    // useMiles = true (displays feet natively)
    utils::adsb::formatAltitudeTag(doc.as<JsonObject>(), buffer, sizeof(buffer), true);
    TEST_ASSERT_EQUAL_STRING("35000 ft", buffer);
    
    // useMiles = false (displays meters)
    utils::adsb::formatAltitudeTag(doc.as<JsonObject>(), buffer, sizeof(buffer), false);
    // 35000 * 0.3048 = 10668
    TEST_ASSERT_EQUAL_STRING("10668 m", buffer);
}

void test_parse_coord(void) {
    double out = 0.0;
    TEST_ASSERT_TRUE(utils::math::parseCoord("12.34", &out));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 12.34f, static_cast<float>(out));
    
    TEST_ASSERT_TRUE(utils::math::parseCoord("-98.76", &out));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -98.76f, static_cast<float>(out));
    
    TEST_ASSERT_FALSE(utils::math::parseCoord("abc", &out));
    TEST_ASSERT_FALSE(utils::math::parseCoord("12.34x", &out));
    TEST_ASSERT_FALSE(utils::math::parseCoord("", &out));
    TEST_ASSERT_FALSE(utils::math::parseCoord(nullptr, &out));
}

void test_valid_lat_lon(void) {
    TEST_ASSERT_TRUE(utils::math::validLatLon(45.0, 90.0));
    TEST_ASSERT_TRUE(utils::math::validLatLon(-90.0, -180.0));
    TEST_ASSERT_TRUE(utils::math::validLatLon(90.0, 180.0));
    
    TEST_ASSERT_FALSE(utils::math::validLatLon(91.0, 0.0));
    TEST_ASSERT_FALSE(utils::math::validLatLon(-91.0, 0.0));
    TEST_ASSERT_FALSE(utils::math::validLatLon(0.0, 181.0));
    TEST_ASSERT_FALSE(utils::math::validLatLon(0.0, -181.0));
}

void test_format_ring3_label(void) {
    char buffer[16];
    utils::math::formatRing3Label(buffer, sizeof(buffer), 10.0f, false);
    TEST_ASSERT_EQUAL_STRING("10km", buffer);
    
    utils::math::formatRing3Label(buffer, sizeof(buffer), 16.0934f, true);
    TEST_ASSERT_EQUAL_STRING("10mi", buffer);
}

void test_equirectangular_offset_km(void) {
    float dx_km = 0, dy_km = 0, dist_km = 0;
    // Test 1: Same point
    utils::math::equirectangularOffsetKm(45.0f, -120.0f, 45.0f, -120.0f, &dx_km, &dy_km, &dist_km);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, dx_km);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, dy_km);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, dist_km);
    
    // Test 2: Moving North 1 degree (should be 111km)
    utils::math::equirectangularOffsetKm(45.0f, -120.0f, 46.0f, -120.0f, &dx_km, &dy_km, &dist_km);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, dx_km);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 111.0f, dy_km);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 111.0f, dist_km);
}

void test_e7_to_deg(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 45.1234567f, utils::math::e7ToDeg(451234567));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, -120.9876543f, utils::math::e7ToDeg(-1209876543));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, utils::math::e7ToDeg(0));
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
    RUN_TEST(test_is_helicopter);
    RUN_TEST(test_format_altitude_tag);
    RUN_TEST(test_parse_coord);
    RUN_TEST(test_valid_lat_lon);
    RUN_TEST(test_format_ring3_label);
    RUN_TEST(test_equirectangular_offset_km);
    RUN_TEST(test_e7_to_deg);
    return UNITY_END();
}
