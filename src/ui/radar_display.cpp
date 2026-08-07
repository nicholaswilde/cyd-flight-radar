#include "ui/radar_display.h"

#include <lgfx/v1/lgfx_fonts.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <time.h>

#include "config.h"
#include "catppuccin.h"
#include "hardware/display.h"
#include "hardware/display_font.h"
#include "services/adsb_client.h"
#include "services/radar_location.h"
#include "ui/radar_range.h"
#include "ui/radar_theme.h"
#include "ui/runway_overlay.h"
#include "ui/settings_menu.h"



namespace ui {
namespace radar {

static uint16_t hex2rgb565(uint32_t hex) {
    uint8_t r = (hex >> 16) & 0xFF;
    uint8_t g = (hex >> 8) & 0xFF;
    uint8_t b = hex & 0xFF;
    if (config::kDisplayRgbOrder) {
        return tft.color565(b, g, r);
    }
    return tft.color565(r, g, b);
}

uint16_t kColorBackground = hex2rgb565(0x040A1C);
uint16_t kColorGrid = hex2rgb565(0x106420);
uint16_t kColorLabel = hex2rgb565(0xFFFFFF);
uint16_t kColorCenter = hex2rgb565(0xFFFFFF);
uint16_t kColorAircraft = hex2rgb565(0xFF0000);
uint16_t kColorMilitary = hex2rgb565(0xFF8000);
uint16_t kColorHelicopter = hex2rgb565(0x00FFFF);
uint16_t kColorTrackVector = hex2rgb565(0xFF00FF);
uint16_t kColorTagType = hex2rgb565(0xFFC800);
uint16_t kColorTagAltitude = hex2rgb565(0x5AC8FF);
uint16_t kColorRunway = hex2rgb565(0x3896AA);
uint16_t kColorRunwayLabel = hex2rgb565(0xFFFFFF);

}  // namespace radar

void updateThemeColors() {
    radar::kColorBackground = radar::hex2rgb565(0x040A1C);
    radar::kColorGrid = radar::hex2rgb565(0x106420);
    radar::kColorLabel = radar::hex2rgb565(0xFFFFFF);
    radar::kColorCenter = radar::hex2rgb565(0xFFFFFF);
    radar::kColorAircraft = radar::hex2rgb565(0xFF0000);
    radar::kColorMilitary = radar::hex2rgb565(0xFF8000);
    radar::kColorHelicopter = radar::hex2rgb565(0x00FFFF);
    radar::kColorTrackVector = radar::hex2rgb565(0xFF00FF);
    radar::kColorTagType = radar::hex2rgb565(0xFFC800);
    radar::kColorTagAltitude = radar::hex2rgb565(0x5AC8FF);
    radar::kColorRunway = radar::hex2rgb565(0x3896AA);
    radar::kColorRunwayLabel = radar::hex2rgb565(0xFFFFFF);
}

namespace {

bool s_label_metrics_ready = false;
bool s_cardinal_use_vlw = false;
bool s_scale_use_vlw = false;
float s_cardinal_vlw_size = 0.56f;
float s_scale_vlw_size = 0.50f;
float s_tag_vlw_size = 0.56f;
const lgfx::GFXfont* s_cardinal_gfx = &fonts::FreeSansBold12pt7b;
const lgfx::GFXfont* s_scale_gfx = &fonts::FreeSansBold9pt7b;
const lgfx::GFXfont* s_tag_gfx = &fonts::FreeSansBold12pt7b;

bool s_tag_label_metrics_ready = false;
bool s_tag_use_vlw = false;

int s_scale_label_max_w = 0;
int s_scale_label_h = 0;

lgfx::LovyanGFX* s_draw = &tft;
LGFX_Sprite s_frame(&tft);
bool s_frame_ready = false;
char s_selected_hex[7] = {0};
bool s_data_stale = false;

class DrawScope {
 public:
  explicit DrawScope(lgfx::LovyanGFX& gfx) : prev_(s_draw) { s_draw = &gfx; }
  ~DrawScope() { s_draw = prev_; }

 private:
  lgfx::LovyanGFX* prev_;
};

int absDiff(int a, int b) { return std::abs(a - b); }

int measureGfxHeight(const lgfx::GFXfont& font) {
  tft.setFont(&font);
  tft.setTextSize(1);
  return tft.fontHeight();
}

int measureVlwHeight(float size) {
  tft.setTextSize(size);
  return tft.fontHeight();
}

float findVlwSizeForHeight(int target_px) {
  float lo = 0.25f;
  float hi = 1.2f;
  for (int i = 0; i < 16; ++i) {
    const float mid = (lo + hi) * 0.5f;
    if (measureVlwHeight(mid) < target_px) {
      lo = mid;
    } else {
      hi = mid;
    }
  }
  return hi;
}

void applyScaleStyle();

const lgfx::GFXfont* pickGfxFontClosest(
    int target_px, const lgfx::GFXfont* const* candidates, size_t count) {
  const lgfx::GFXfont* best = candidates[0];
  int best_diff = absDiff(measureGfxHeight(*best), target_px);

  for (size_t i = 1; i < count; ++i) {
    const int diff = absDiff(measureGfxHeight(*candidates[i]), target_px);
    if (diff < best_diff) {
      best_diff = diff;
      best = candidates[i];
    }
  }
  return best;
}

void initLabelMetrics() {
  if (s_label_metrics_ready) {
    return;
  }

  const int cardinal_target = radar::kCardinalLabelHeightPx;

  if (displayFontIsSmooth()) {
    s_cardinal_use_vlw = true;
    s_cardinal_vlw_size = findVlwSizeForHeight(cardinal_target);
    const int cardinal_h = measureVlwHeight(s_cardinal_vlw_size);
    const int scale_target = cardinal_h - radar::kScaleBelowCardinalPx;
    s_scale_use_vlw = true;
    s_scale_vlw_size = findVlwSizeForHeight(scale_target);
  } else {
    const lgfx::GFXfont* cardinal_candidates[] = {&fonts::FreeSansBold12pt7b,
                                                  &fonts::FreeSansBold9pt7b};
    s_cardinal_gfx =
        pickGfxFontClosest(cardinal_target, cardinal_candidates, 2);
    s_cardinal_use_vlw = false;

    const int cardinal_h = measureGfxHeight(*s_cardinal_gfx);
    const int scale_target = cardinal_h - radar::kScaleBelowCardinalPx;
    const lgfx::GFXfont* scale_candidates[] = {&fonts::FreeSansBold9pt7b,
                                               &fonts::FreeSansBold12pt7b};
    s_scale_gfx = pickGfxFontClosest(scale_target, scale_candidates, 2);
    s_scale_use_vlw = false;
  }

  applyScaleStyle();
  s_scale_label_h = tft.fontHeight();
  s_scale_label_max_w = 0;
  char label[12];
  for (size_t i = 0; i < radar::kRangePresetCount; ++i) {
    for (bool miles : {false, true}) {
      radar::formatRing3Label(label, sizeof(label), radar::kRangePresets[i].ring3_km,
                              miles);
      const int w = tft.textWidth(label);
      if (w > s_scale_label_max_w) {
        s_scale_label_max_w = w;
      }
    }
  }

  s_label_metrics_ready = true;
}

void initTagLabelMetrics() {
  if (s_tag_label_metrics_ready) {
    return;
  }

  const int target = radar::kAircraftTagLabelHeightPx;
  if (displayFontIsSmooth()) {
    s_tag_use_vlw = true;
    s_tag_vlw_size = findVlwSizeForHeight(target);
  } else {
    const lgfx::GFXfont* tag_candidates[] = {&fonts::FreeSansBold12pt7b,
                                               &fonts::FreeSansBold9pt7b};
    s_tag_gfx = pickGfxFontClosest(target, tag_candidates, 2);
    s_tag_use_vlw = false;
  }

  s_tag_label_metrics_ready = true;
}

void initPalette() {
  updateThemeColors();
}

constexpr float kKmPerDeg = 111.0f;
constexpr float kDegToRad = 3.14159265f / 180.0f;

void offsetKmFromCenter(float lat, float lon, float* dx_km, float* dy_km,
                        float* dist_km) {
  // Longitude degrees shrink toward the poles; scale by cos(latitude) so
  // east-west distance isn't overstated away from the equator.
  const float center_lat_rad =
      static_cast<float>(services::location::lat()) * kDegToRad;
  *dx_km = static_cast<float>(lon - services::location::lon()) * kKmPerDeg *
           cosf(center_lat_rad);
  *dy_km =
      static_cast<float>(lat - services::location::lat()) * kKmPerDeg;
  *dist_km = sqrtf((*dx_km) * (*dx_km) + (*dy_km) * (*dy_km));
}

float innerRingMaxKm() {
  const float outer_km = radar::rangeCurrent().outer_km;
  return outer_km * (static_cast<float>(radar::kGridOuterRadius -
                                       radar::kAircraftInsideRingInsetPx) /
                     static_cast<float>(radar::kGridOuterRadius));
}

/** Flat lat/lon as x/y: 1° ≈ 111 km, north = screen up. */
void latLonToScreen(float lat, float lon, int* out_x, int* out_y) {
  const float outer_km = radar::rangeCurrent().outer_km;
  const float px_per_km = static_cast<float>(radar::kGridOuterRadius) / outer_km;

  float dx_km = 0.0f;
  float dy_km = 0.0f;
  float dist_km = 0.0f;
  offsetKmFromCenter(lat, lon, &dx_km, &dy_km, &dist_km);

  *out_x = radar::kCenterX + static_cast<int>(lroundf(dx_km * px_per_km));
  *out_y = radar::kCenterY - static_cast<int>(lroundf(dy_km * px_per_km));
}

bool isInsideOuterRingKm(float dist_km) { return dist_km <= innerRingMaxKm(); }

int distSqFromCenter(int x, int y) {
  const int dx = x - radar::kCenterX;
  const int dy = y - radar::kCenterY;
  return dx * dx + dy * dy;
}

bool isInsideOuterRing(int x, int y) {
  const int max_r = radar::kGridOuterRadius - radar::kAircraftInsideRingInsetPx;
  return distSqFromCenter(x, y) <= max_r * max_r;
}

/** Rim dot from true bearing; always on screen edge (even if target is 50+ km away). */
bool beyondRingEdgeDotFromLatLon(float lat, float lon, int* out_x, int* out_y) {
  float dx_km = 0.0f;
  float dy_km = 0.0f;
  float dist_km = 0.0f;
  offsetKmFromCenter(lat, lon, &dx_km, &dy_km, &dist_km);
  if (dist_km < 0.01f) {
    return false;
  }
  if (isInsideOuterRingKm(dist_km)) {
    return false;
  }

  const int cx = radar::kCenterX;
  const int cy = radar::kCenterY;
  const int rim_r = radar::kCenterX - radar::kBeyondRingScreenMarginPx;
  const float angle_rad = atan2f(dx_km, dy_km);

  *out_x = cx + static_cast<int>(lroundf(sinf(angle_rad) * rim_r));
  *out_y = cy - static_cast<int>(lroundf(cosf(angle_rad) * rim_r));
  return true;
}

void drawBeyondRingDot(int x, int y, uint16_t color) {
  s_draw->fillSmoothCircle(x, y, radar::kBeyondRingDotRadiusPx, color);
}

void clipPointToOuterRing(int x0, int y0, int* x1, int* y1) {
  const int max_r = radar::kGridOuterRadius;
  const int max_r_sq = max_r * max_r;
  if (distSqFromCenter(*x1, *y1) <= max_r_sq) {
    return;
  }

  const int dx = *x1 - x0;
  const int dy = *y1 - y0;
  float t = 1.0f;
  for (int step = 0; step < 20; ++step) {
    const int px = x0 + static_cast<int>(lroundf(dx * t));
    const int py = y0 + static_cast<int>(lroundf(dy * t));
    if (distSqFromCenter(px, py) <= max_r_sq) {
      *x1 = px;
      *y1 = py;
      return;
    }
    t -= 0.05f;
    if (t <= 0.0f) {
      *x1 = x0;
      *y1 = y0;
      return;
    }
  }
}

int speedLineLengthPx(float gs_knots) {
  if (gs_knots <= 0.0f) {
    return 0;
  }

  // Fixed screen scale: 60 s horizon at gs, not tied to current range zoom.
  constexpr float kKmPerKnotPerHorizon =
      1.852f * radar::kAircraftTrackHorizonSec / 3600.0f;
  const float px =
      gs_knots * kKmPerKnotPerHorizon * radar::kGridOuterRadius /
      radar::kAircraftTrackRefOuterKm * radar::kAircraftTrackLengthScale;

  const int len = static_cast<int>(px + 0.5f);
  if (len < radar::kAircraftSpeedLineMinPx) {
    return radar::kAircraftSpeedLineMinPx;
  }
  return len;
}

void noseTip(int cx, int cy, float heading_deg, int* tip_x, int* tip_y) {
  constexpr float kDegToRad = 0.01745329252f;
  const float rad = heading_deg * kDegToRad;
  *tip_x = cx + static_cast<int>(lroundf(sinf(rad) * radar::kAircraftNoseLenPx));
  *tip_y = cy - static_cast<int>(lroundf(cosf(rad) * radar::kAircraftNoseLenPx));
}

void drawHeadingTriangle(int cx, int cy, float heading_deg, uint16_t color) {
  constexpr float kDegToRad = 0.01745329252f;
  const float rad = heading_deg * kDegToRad;
  const float sin_h = sinf(rad);
  const float cos_h = cosf(rad);

  int tip_x = 0;
  int tip_y = 0;
  noseTip(cx, cy, heading_deg, &tip_x, &tip_y);

  const int base_x =
      cx - static_cast<int>(lroundf(sin_h * static_cast<float>(radar::kAircraftTailLenPx)));
  const int base_y =
      cy + static_cast<int>(lroundf(cos_h * static_cast<float>(radar::kAircraftTailLenPx)));

  const int wing_x = static_cast<int>(lroundf(cos_h * radar::kAircraftTailHalfPx));
  const int wing_y = static_cast<int>(lroundf(sin_h * radar::kAircraftTailHalfPx));

  s_draw->fillTriangle(tip_x, tip_y, base_x + wing_x, base_y + wing_y,
                       base_x - wing_x, base_y - wing_y, color);
}

void drawHelicopterSymbol(int cx, int cy, float heading_deg, uint16_t color) {
  constexpr float kDegToRad = 0.01745329252f;
  const float rad = heading_deg * kDegToRad;
  const float sin_h = sinf(rad);
  const float cos_h = cosf(rad);

  s_draw->fillSmoothCircle(cx, cy, radar::kAircraftTailLenPx, color);
  
  const int tail_x = cx - static_cast<int>(lroundf(sin_h * radar::kAircraftNoseLenPx));
  const int tail_y = cy + static_cast<int>(lroundf(cos_h * radar::kAircraftNoseLenPx));
  s_draw->drawWideLine(cx, cy, tail_x, tail_y, radar::kAircraftTrackLineHalfWidth, color);
}

void drawSpeedVector(int cx, int cy, float heading_deg, float track_deg,
                     float gs_knots, uint16_t color) {
  const int len = speedLineLengthPx(gs_knots);
  if (len <= 0) {
    return;
  }

  int tip_x = 0;
  int tip_y = 0;
  noseTip(cx, cy, heading_deg, &tip_x, &tip_y);

  constexpr float kDegToRad = 0.01745329252f;
  const float rad = track_deg * kDegToRad;
  int ex = tip_x + static_cast<int>(lroundf(sinf(rad) * len));
  int ey = tip_y - static_cast<int>(lroundf(cosf(rad) * len));
  clipPointToOuterRing(tip_x, tip_y, &ex, &ey);
  if (ex == tip_x && ey == tip_y) {
    return;
  }
  s_draw->drawWideLine(tip_x, tip_y, ex, ey, radar::kAircraftTrackLineHalfWidth,
                       color);
}

void drawSelectedHighlight(int cx, int cy) {
  s_draw->drawCircle(cx, cy, 14, radar::kColorTagType);
  s_draw->drawCircle(cx, cy, 15, radar::kColorTagType);
}

void applyTagStyle() {
  if (s_tag_use_vlw) {
    displayFontSetSmoothSize(*s_draw, s_tag_vlw_size);
  } else {
    displayFontSetBitmap(*s_draw, s_tag_gfx);
  }
}

int measureTagBlockWidth(const services::adsb::Aircraft& plane) {
  applyTagStyle();
  int max_w = 0;
  if (plane.callsign[0] != '\0') {
    const int w = s_draw->textWidth(plane.callsign);
    if (w > max_w) {
      max_w = w;
    }
  }
  if (plane.type[0] != '\0') {
    const int w = s_draw->textWidth(plane.type);
    if (w > max_w) {
      max_w = w;
    }
  }
  if (plane.alt[0] != '\0') {
    const int w = s_draw->textWidth(plane.alt);
    if (w > max_w) {
      max_w = w;
    }
  }
  return max_w;
}

void drawAircraftTag(int x, int y, const services::adsb::Aircraft& plane) {
  initTagLabelMetrics();
  applyTagStyle();

  const int line_h = s_draw->fontHeight();
  const int block_w = measureTagBlockWidth(plane);
  const int block_h = line_h * 3;
  int ly = y - block_h / 2;

  const int symbol_half =
      radar::kAircraftNoseLenPx + radar::kAircraftTailHalfPx;
  // West (left): tag toward center on the right; east (right): tag on the left.
  const bool tag_on_right = x < radar::kCenterX;
  int anchor_x = 0;
  if (tag_on_right) {
    anchor_x = x + symbol_half + radar::kAircraftLabelGapPx;
    anchor_x = std::min(anchor_x, radar::kSize - block_w - 1);
    s_draw->setTextDatum(textdatum_t::top_left);
  } else {
    anchor_x = x - symbol_half - radar::kAircraftLabelGapPx;
    anchor_x = std::max(anchor_x, block_w + 1);
    s_draw->setTextDatum(textdatum_t::top_right);
  }
  ly = std::max(1, std::min(ly, radar::kSize - block_h - 1));

  if (plane.callsign[0] != '\0') {
    s_draw->setTextColor(radar::kColorLabel, radar::kColorBackground);
    s_draw->drawString(plane.callsign, anchor_x, ly);
  }
  ly += line_h;

  if (plane.type[0] != '\0') {
    s_draw->setTextColor(radar::kColorTagType, radar::kColorBackground);
    s_draw->drawString(plane.type, anchor_x, ly);
  }
  ly += line_h;

  if (plane.alt[0] != '\0') {
    s_draw->setTextColor(radar::kColorTagAltitude, radar::kColorBackground);
    s_draw->drawString(plane.alt, anchor_x, ly);
  }
}

struct AircraftDrawItem {
  size_t index = 0;
  int x = 0;
  int y = 0;
  int dist_sq = 0;
};

struct BeyondDotDrawItem {
  int x = 0;
  int y = 0;
  int dist_sq = 0;
  bool is_heli = false;
  bool is_military = false;
};

void sortDrawItemsFarFirst(AircraftDrawItem* items, size_t count) {
  for (size_t i = 1; i < count; ++i) {
    const AircraftDrawItem key = items[i];
    size_t j = i;
    while (j > 0 && items[j - 1].dist_sq < key.dist_sq) {
      items[j] = items[j - 1];
      --j;
    }
    items[j] = key;
  }
}

void sortBeyondDotsFarFirst(BeyondDotDrawItem* items, size_t count) {
  for (size_t i = 1; i < count; ++i) {
    const BeyondDotDrawItem key = items[i];
    size_t j = i;
    while (j > 0 && items[j - 1].dist_sq < key.dist_sq) {
      items[j] = items[j - 1];
      --j;
    }
    items[j] = key;
  }
}

void drawAircraft() {
  initLabelMetrics();

  const size_t n = services::adsb::aircraftCount();
  const services::adsb::Aircraft* planes = services::adsb::aircraftList();

  AircraftDrawItem items[services::adsb::kMaxAircraft];
  BeyondDotDrawItem dots[services::adsb::kMaxAircraft];
  size_t draw_count = 0;
  size_t dot_count = 0;

  for (size_t i = 0; i < n; ++i) {
    float dx_km = 0.0f;
    float dy_km = 0.0f;
    float dist_km = 0.0f;
    offsetKmFromCenter(planes[i].lat, planes[i].lon, &dx_km, &dy_km, &dist_km);

    if (isInsideOuterRingKm(dist_km)) {
      int x = 0;
      int y = 0;
      latLonToScreen(planes[i].lat, planes[i].lon, &x, &y);
      items[draw_count].index = i;
      items[draw_count].x = x;
      items[draw_count].y = y;
      items[draw_count].dist_sq = distSqFromCenter(x, y);
      ++draw_count;
      continue;
    }

    int dot_x = 0;
    int dot_y = 0;
    if (!beyondRingEdgeDotFromLatLon(planes[i].lat, planes[i].lon, &dot_x,
                                     &dot_y)) {
      continue;
    }
    dots[dot_count].x = dot_x;
    dots[dot_count].y = dot_y;
    dots[dot_count].dist_sq = distSqFromCenter(dot_x, dot_y);
    dots[dot_count].is_heli = planes[i].is_heli;
    dots[dot_count].is_military = planes[i].is_military;
    ++dot_count;
  }

  sortBeyondDotsFarFirst(dots, dot_count);
  for (size_t d = 0; d < dot_count; ++d) {
    drawBeyondRingDot(dots[d].x, dots[d].y, 
        dots[d].is_heli ? radar::kColorHelicopter : 
        (dots[d].is_military ? radar::kColorMilitary : radar::kColorAircraft));
  }

  sortDrawItemsFarFirst(items, draw_count);
  for (size_t d = 0; d < draw_count; ++d) {
    const size_t i = items[d].index;
    const int x = items[d].x;
    const int y = items[d].y;
    
    if (s_selected_hex[0] != '\0' && strcmp(planes[i].hex, s_selected_hex) == 0) {
      drawSelectedHighlight(x, y);
    }
    
    drawSpeedVector(x, y, planes[i].nose_deg, planes[i].track_deg,
                    planes[i].gs_knots, radar::kColorTrackVector);
    if (planes[i].is_heli) {
      drawHelicopterSymbol(x, y, planes[i].nose_deg, radar::kColorHelicopter);
    } else {
      drawHeadingTriangle(x, y, planes[i].nose_deg, 
          planes[i].is_military ? radar::kColorMilitary : radar::kColorAircraft);
    }
  }
  for (size_t d = 0; d < draw_count; ++d) {
    const size_t i = items[d].index;
    drawAircraftTag(items[d].x, items[d].y, planes[i]);
  }
}

void applyCardinalStyle() {
  if (s_cardinal_use_vlw) {
    displayFontSetSmoothSize(*s_draw, s_cardinal_vlw_size);
  } else {
    displayFontSetBitmap(*s_draw, s_cardinal_gfx);
  }
}

void applyScaleStyle() {
  if (s_scale_use_vlw) {
    displayFontSetSmoothSize(*s_draw, s_scale_vlw_size);
  } else {
    displayFontSetBitmap(*s_draw, s_scale_gfx);
  }
}

void drawCardinalLabel(const char* text, int x, int y, textdatum_t datum) {
  applyCardinalStyle();
  s_draw->setTextDatum(datum);
  s_draw->setTextColor(radar::kColorLabel, radar::kColorBackground);
  s_draw->drawString(text, x, y);
}

void drawScaleLabelWithBackground(const char* text, int x, int y) {
  applyScaleStyle();
  s_draw->setTextDatum(textdatum_t::middle_right);

  const int tw = s_draw->textWidth(text);
  const int th = s_draw->fontHeight();
  constexpr int kPadX = 3;
  constexpr int kPadY = 2;

  const int left = x - tw - kPadX;
  const int top = y - th / 2 - kPadY;

  s_draw->fillRect(left, top, tw + kPadX * 2, th + kPadY * 2,
                   radar::kColorBackground);
  s_draw->setTextColor(radar::kColorGrid, radar::kColorBackground);
  s_draw->drawString(text, x, y);
}

void drawGridRing(int cx, int cy, int r, uint16_t color) {
  if (r <= 0) {
    return;
  }
  const int thickness =
      std::max(1, static_cast<int>(radar::kGridStrokeHalfWidth * 2.0f));
  for (int i = 0; i < thickness && r - i > 0; ++i) {
    s_draw->drawCircle(cx, cy, r - i, color);
  }
}

void drawRings(int cx, int cy, int outer_radius) {
  for (int i = 1; i <= radar::kRingCount; ++i) {
    const int r = (outer_radius * i) / radar::kRingCount;
    drawGridRing(cx, cy, r, radar::kColorGrid);
  }
}

void drawCrosshairs(int cx, int cy, int radius, uint16_t color) {
  s_draw->drawWideLine(cx, cy - radius, cx, cy + radius,
                       radar::kGridStrokeHalfWidth, color);
  s_draw->drawWideLine(cx - radius, cy, cx + radius, cy,
                       radar::kGridStrokeHalfWidth, color);
}

void drawCenterDot(int cx, int cy) {
  s_draw->fillSmoothCircle(cx, cy, radar::kCenterDotRadius, radar::kColorCenter);
}

void drawCardinalLabels() {
  const int cx = radar::kCenterX;
  const int cy = radar::kCenterY;
  const int edge = radar::kSize - 1;
  const int margin = 4; // Margin from the edge

  drawCardinalLabel("N", cx, radar::kCardinalNorthOffsetY + margin, textdatum_t::top_center);
  drawCardinalLabel("S", cx, edge + radar::kCardinalSouthOffsetY - margin,
                    textdatum_t::bottom_center);
  drawCardinalLabel("W", margin, cy, textdatum_t::middle_left);
  drawCardinalLabel("E", edge - margin, cy, textdatum_t::middle_right);
}

int scaleLabelAnchorX(int cx, int outer_radius) {
  return cx + outer_radius - radar::kScaleGapFromOuterRing;
}

void drawScaleLabel(int cx, int cy, int outer_radius) {
  char scale_label[12];
  radar::formatCurrentRing3Label(scale_label, sizeof(scale_label));
  drawScaleLabelWithBackground(scale_label,
                               scaleLabelAnchorX(cx, outer_radius), cy);
}

template <typename Gfx>
void drawStaticGrid(Gfx& gfx) {
  initLabelMetrics();
  const DrawScope scope(gfx);
  displayFontEnsureLoaded(gfx);
  const int cx = radar::kCenterX;
  const int cy = radar::kCenterY;
  const int grid_r = radar::kGridOuterRadius;

  gfx.fillScreen(radar::kColorBackground);
  drawRings(cx, cy, grid_r);
  drawCrosshairs(cx, cy, grid_r, radar::kColorGrid);
  initPalette();
  if (settings::isAirportsEnabled()) {
    runway::drawLargeAirportRunways(gfx);
  }
  if (settings::isMediumAirportsEnabled()) {
    runway::drawMediumAirportRunways(gfx);
  }
  drawCenterDot(cx, cy);
  drawCardinalLabels();
  drawScaleLabel(cx, cy, grid_r);
  gfx.setTextDatum(textdatum_t::top_left);
}

bool ensureFrameSprite() {
  if (s_frame_ready) {
    return true;
  }
  static bool s_frame_alloc_failed = false;
  if (s_frame_alloc_failed) {
    return false;
  }
  // Use 8-bit color (RGB332) to halve the memory requirement (57 KB instead of 115 KB).
  // This allows the full 240x240 sprite to fit in the ESP32's fragmented SRAM!
  s_frame.setColorDepth(8);
  if (!s_frame.createSprite(radar::kSize, radar::kSize)) {
    Serial.println("radar: frame sprite alloc failed. Falling back to direct draw (expect flicker).");
    s_frame_alloc_failed = true;
    return false;
  }
  s_frame_ready = true;
  return true;
}

void drawRadarSweep(lgfx::LovyanGFX& gfx) {
  if (!settings::isRadarSweepEnabled()) {
    return;
  }

  const int cx = radar::kCenterX;
  const int cy = radar::kCenterY;
  const int r = radar::kGridOuterRadius;

  const uint32_t t_ms = static_cast<uint32_t>(settings::getSweepRotationSpeedMs());
  const float speed = 360.0f / t_ms;
  const float current_angle_deg = (millis() % t_ms) * speed;
  const float current_angle_rad = current_angle_deg * M_PI / 180.0f;
  
  // Draw fading trail
  for (int i = 20; i >= 0; --i) {
    const float trail_angle_deg = current_angle_deg - i * 1.5f;
    const float trail_angle_rad = trail_angle_deg * M_PI / 180.0f;
    const int tx = cx + static_cast<int>(lroundf(sinf(trail_angle_rad) * r));
    const int ty = cy - static_cast<int>(lroundf(cosf(trail_angle_rad) * r));
    
    // Blend from background to green
    uint32_t bg_hex = 0x040A1C;
    uint32_t fg_hex = 0x00FF00;
    uint8_t bg_r = (bg_hex >> 16) & 0xFF;
    uint8_t bg_g = (bg_hex >> 8) & 0xFF;
    uint8_t bg_b = bg_hex & 0xFF;
    uint8_t fg_r = (fg_hex >> 16) & 0xFF;
    uint8_t fg_g = (fg_hex >> 8) & 0xFF;
    uint8_t fg_b = fg_hex & 0xFF;

    const float t = 1.0f - (static_cast<float>(i) / 20.0f);
    const uint8_t red = static_cast<uint8_t>(bg_r + (fg_r - bg_r) * t);
    const uint8_t green = static_cast<uint8_t>(bg_g + (fg_g - bg_g) * t);
    const uint8_t blue = static_cast<uint8_t>(bg_b + (fg_b - bg_b) * t);
    
    if (config::kDisplayRgbOrder) {
        gfx.drawLine(cx, cy, tx, ty, tft.color565(blue, green, red));
    } else {
        gfx.drawLine(cx, cy, tx, ty, tft.color565(red, green, blue));
    }
  }
}

// Double-buffered frame: composite the grid AND aircraft into the off-screen
// sprite, then blit it to the panel in a single pushSprite. Because the panel
// is updated in one pass, labels never show an erase/redraw gap — no flicker.
void renderFrame() {
  drawStaticGrid(s_frame);  // opens its own DrawScope(s_frame)
  drawRadarSweep(s_frame);
  {
    const DrawScope scope(s_frame);
    drawAircraft();
  }
  s_frame.pushSprite(0, 0);
  tft.setTextDatum(textdatum_t::top_left);
}

uint16_t downsampleColor(uint16_t c) {
  uint8_t r3 = (c >> 13) & 0x07;
  uint8_t g3 = (c >> 8) & 0x07;
  uint8_t b2 = (c >> 3) & 0x03;
  uint16_t r5 = (r3 << 2) | (r3 >> 1);
  uint16_t g6 = (g3 << 3) | (g3 << 1) | (g3 >> 2);
  uint16_t b5 = (b2 << 3) | (b2 << 1) | (b2 >> 1);
  return (r5 << 11) | (g6 << 5) | b5;
}

void drawAppVersion() {
  tft.setTextDatum(textdatum_t::bottom_right);
  uint16_t bg = s_frame_ready ? downsampleColor(radar::kColorBackground) : radar::kColorBackground;
  tft.setTextColor(lgfx::color565(120, 120, 120), bg);
  displayFontEnsureLoaded(tft);
  if (displayFontIsSmooth()) {
    displayFontSetSmoothSize(tft, 0.60f);
  } else {
    displayFontSetBitmap(tft, &fonts::FreeSansBold12pt7b);
  }
  tft.drawString(config::kAppVersion, 238, 318);
}

void drawClock() {
  struct tm timeinfo;
  char time_str[16];
  
  if (!getLocalTime(&timeinfo, 0)) {
    strcpy(time_str, "--:--");
  } else {
    strftime(time_str, sizeof(time_str), "%H:%M", &timeinfo);
  }
  
  tft.setTextDatum(textdatum_t::bottom_left);
  uint16_t bg = s_frame_ready ? downsampleColor(radar::kColorBackground) : radar::kColorBackground;
  tft.setTextColor(lgfx::color565(120, 120, 120), bg);
  displayFontEnsureLoaded(tft);
  if (displayFontIsSmooth()) {
    displayFontSetSmoothSize(tft, 0.60f);
  } else {
    displayFontSetBitmap(tft, &fonts::FreeSansBold12pt7b);
  }
  tft.drawString(time_str, 2, 318);
}

void drawDetailsPanel() {
  uint16_t bg = s_frame_ready ? downsampleColor(radar::kColorBackground) : radar::kColorBackground;
  tft.fillRect(0, radar::kSize, 240, 320 - radar::kSize, bg);
  tft.drawFastHLine(0, radar::kSize, 240, radar::kColorGrid);
  drawAppVersion();
  drawClock();

  if (s_data_stale) {
    tft.fillRect(0, 320 - 24, 240, 24, lgfx::color565(200, 0, 0));
    tft.setTextColor(lgfx::color565(255, 255, 255), lgfx::color565(200, 0, 0));
    tft.setTextDatum(textdatum_t::middle_center);
    displayFontEnsureLoaded(tft);
    if (displayFontIsSmooth()) {
      displayFontSetSmoothSize(tft, 0.7f);
    } else {
      displayFontSetBitmap(tft, &fonts::FreeSansBold9pt7b);
    }
    tft.drawString("Connection Lost", 120, 320 - 12);
    // Draw on top of everything at the bottom of the screen
  }

  if (s_selected_hex[0] == '\0') {
    tft.setTextDatum(textdatum_t::middle_center);
    tft.setTextColor(radar::kColorLabel, bg);
    displayFontEnsureLoaded(tft);
    if (displayFontIsSmooth()) {
      displayFontSetSmoothSize(tft, 1.2f);
    } else {
      displayFontSetBitmap(tft, &fonts::FreeSansBold12pt7b);
    }
    tft.drawString("CYD Flight Radar", 120, radar::kSize + (320 - radar::kSize) / 2);
    return;
  }
  
  const services::adsb::Aircraft* planes = services::adsb::aircraftList();
  const size_t n = services::adsb::aircraftCount();
  const services::adsb::Aircraft* ac = nullptr;
  for (size_t i = 0; i < n; ++i) {
    if (strcmp(planes[i].hex, s_selected_hex) == 0) {
      ac = &planes[i];
      break;
    }
  }
  
  if (!ac) return; // Selected aircraft disappeared
  
  tft.setTextDatum(textdatum_t::top_left);
  displayFontEnsureLoaded(tft);
  if (displayFontIsSmooth()) {
    displayFontSetSmoothSize(tft, 0.8f); // Increased from 0.5f for better readability
  } else {
    displayFontSetBitmap(tft, &fonts::FreeSansBold9pt7b);
  }
  
  const int lh = tft.fontHeight();
  int ly = radar::kSize + 4;
  char buf[64];
  
  tft.setTextColor(radar::kColorLabel, bg);
  if (ac->reg[0] != '\0') {
    snprintf(buf, sizeof(buf), "%s (%s)%s", ac->callsign, ac->reg, ac->is_military ? " [MIL]" : "");
  } else {
    if (ac->callsign[0] == '~' || ac->callsign[0] == '\0') {
      snprintf(buf, sizeof(buf), "Hex: %s%s", ac->hex, ac->is_military ? " [MIL]" : "");
    } else {
      snprintf(buf, sizeof(buf), "%s%s", ac->callsign, ac->is_military ? " [MIL]" : "");
    }
  }
  tft.drawString(buf, 4, ly);
  ly += lh;
  
  tft.setTextColor(radar::kColorTagType, bg);
  if (ac->desc[0] != '\0') {
    tft.drawString(ac->desc, 4, ly);
  } else if (ac->type[0] != '\0') {
    tft.drawString(ac->type, 4, ly);
  } else {
    tft.drawString("Unknown Aircraft Type", 4, ly);
  }
  ly += lh;
  
  tft.setTextColor(radar::kColorTagAltitude, bg);
  snprintf(buf, sizeof(buf), "%s | %.0f kts", ac->alt, ac->gs_knots);
  tft.drawString(buf, 4, ly);
  ly += lh;
  
  float dx, dy, dist;
  offsetKmFromCenter(ac->lat, ac->lon, &dx, &dy, &dist);
  snprintf(buf, sizeof(buf), "Dst: %.1f km (%.1f mi)", dist, dist / 1.60934f);
  tft.drawString(buf, 4, ly);
}

}  // namespace

void radarDisplaySetStale(bool is_stale) {
  if (s_data_stale != is_stale) {
    s_data_stale = is_stale;
    radarDisplayRefreshAircraft();
  }
}

bool radarDisplayHandleTouch(int x, int y) {
  bool changed = false;
  if (y >= radar::kSize) {
    if (s_selected_hex[0] != '\0') {
      s_selected_hex[0] = '\0';
      changed = true;
    }
    return changed;
  }
  
  const services::adsb::Aircraft* planes = services::adsb::aircraftList();
  const size_t n = services::adsb::aircraftCount();
  int best_dist_sq = 2500; // ~50px radius to make it easier to hit with fat fingers
  const char* best_hex = nullptr;
  
  for (size_t i = 0; i < n; ++i) {
    int px, py;
    latLonToScreen(planes[i].lat, planes[i].lon, &px, &py);
    int d_sq = (px - x) * (px - x) + (py - y) * (py - y);
    if (d_sq < best_dist_sq) {
      best_dist_sq = d_sq;
      best_hex = planes[i].hex;
    }
  }
  
  if (best_hex) {
    if (strcmp(s_selected_hex, best_hex) != 0) {
      strcpy(s_selected_hex, best_hex);
      changed = true;
    }
    return true; // Handled
  } else {
    if (s_selected_hex[0] != '\0') {
      s_selected_hex[0] = '\0';
      changed = true;
      return true; // Handled
    }
  }
  return false; // Not handled, pass through
}

void radarDisplayDraw() {
  initPalette();
  initLabelMetrics();

  // Clear the full screen once to erase any previous status screens
  tft.fillRect(0, 0, config::kDisplayWidth, config::kDisplayHeight,
               radar::kColorBackground);

  if (ensureFrameSprite()) {
    renderFrame();
    drawDetailsPanel();
    return;
  }

  // Fallback when the sprite can't be allocated: draw straight to the panel.
  const DrawScope scope(tft);
  drawStaticGrid(tft);
  drawAircraft();
  tft.setTextDatum(textdatum_t::top_left);
  drawDetailsPanel();
}

void radarDisplayUpdateAnimation() {
  initPalette();
  if (ensureFrameSprite()) {
    renderFrame();
  }
  
  // Update clock independently of ADS-B fetches
  static int s_last_minute = -1;
  struct tm timeinfo;
  if (getLocalTime(&timeinfo, 0)) {
    if (timeinfo.tm_min != s_last_minute) {
      s_last_minute = timeinfo.tm_min;
      drawClock();
    }
  } else {
    if (s_last_minute != -2) {
      s_last_minute = -2;
      drawClock();
    }
  }
}

void radarDisplayRefreshAircraft() {
  initPalette();

  if (ensureFrameSprite()) {
    renderFrame();
    drawDetailsPanel();
    return;
  }

  radarDisplayDraw();
}

}  // namespace ui
