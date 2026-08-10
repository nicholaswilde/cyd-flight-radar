#pragma once

#include <cstddef>

namespace services::adsb {

struct Aircraft {
  char hex[7];
  float lat;
  float lon;
  float nose_deg;
  float track_deg;
  float gs_knots;
  char callsign[9];
  char type[5];
  char alt[12];
  char reg[10];
  char desc[32];
  bool is_heli;
  bool is_military;
  
  char route_origin[5];
  char route_destination[5];
  
  struct {
    float lat;
    float lon;
  } trail[6];
  size_t trail_size;
  size_t trail_head;
};

constexpr size_t kMaxAircraft = 40;

size_t aircraftCount();
const Aircraft* aircraftList();


/**
 * Pre-allocate static state before WiFi starts.
 * MUST be called in setup() before wifiManager.begin().
 *
 * ArduinoJson v7 uses ARDUINOJSON_POOL_CAPACITY=256 slots per pool block
 * (4096 bytes each on 32-bit). The filterDoc() static JsonDocument allocates
 * its first pool on first call. If that happens inside fetchUpdate() (after
 * WiFi claims ~100KB), the 4KB+ pool lands in the only large free block
 * (~38900 bytes), permanently reducing largest_free to 32756 — below the
 * 36KB SSL minimum, causing -32512 on every subsequent fetch.
 * Calling init() before WiFi places the pool at the bottom of the clean heap.
 */
void init();

/** Fetch aircraft within fetch_radius_km of center_lat/lon from adsb.fi. */
bool fetchUpdate(double center_lat, double center_lon, float fetch_radius_km);

/** Fetch route for a specific aircraft from adsbdb.com on the next update. */
void requestRouteFetch(const char* callsign, const char* hex);

}  // namespace services::adsb
