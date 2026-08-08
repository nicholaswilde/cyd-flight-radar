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


/** Fetch aircraft within fetch_radius_km of center_lat/lon from adsb.fi. */
bool fetchUpdate(double center_lat, double center_lon, float fetch_radius_km);

/** Fetch route for a specific aircraft from adsbdb.com. */
void fetchRoute(Aircraft* ac);

}  // namespace services::adsb
