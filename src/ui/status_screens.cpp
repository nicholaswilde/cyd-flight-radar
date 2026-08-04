#include "ui/status_screens.h"

#include <lgfx/v1/lgfx_fonts.hpp>

#include <cmath>
#include <cstdio>
#include <cstddef>
#include <cstring>

#include "config.h"
#include "hardware/display.h"
#include "hardware/display_font.h"
#include "ui/settings_menu.h"
#include "catppuccin.h"

static uint16_t hex2rgb565(uint32_t hex) {
    uint8_t r = (hex >> 16) & 0xFF;
    uint8_t g = (hex >> 8) & 0xFF;
    uint8_t b = hex & 0xFF;
    if (config::kDisplayRgbOrder) {
        return tft.color565(b, g, r);
    }
    return tft.color565(r, g, b);
}




namespace {

constexpr int kLineGap = 6;
const int kCenterX = config::kDisplayWidth / 2;
const int kCenterY = config::kDisplayHeight / 2;

constexpr int kSpinnerDotCount = 10;
constexpr int kSpinnerRadius = 113;
constexpr int kSpinnerDotRadius = 2;
constexpr int kSpinnerEraseRadius = 4;
constexpr float kSpinnerStepDeg = 6.0f;

struct SpinnerDot {
  int x = 0;
  int y = 0;
  bool drawn = false;
};

char s_connecting_ssid[33];
char s_ssid_line[33];
constexpr int kConnectingTextMaxWidthPx = 220;
float s_spinner_angle_deg = -90.0f;
SpinnerDot s_spinner_dots[kSpinnerDotCount];
bool s_connecting_text_drawn = false;

constexpr auto& kGfxTitle = fonts::FreeSans18pt7b;
constexpr auto& kGfxBody = fonts::FreeSans12pt7b;
constexpr auto& kGfxDetail = fonts::Font2;
constexpr auto& kPortalGfxTitle = fonts::FreeSansBold18pt7b;
constexpr auto& kPortalGfxBody = fonts::FreeSansBold12pt7b;
constexpr auto& kPortalGfxEmphasis = fonts::FreeSansBold18pt7b;
constexpr auto& kConnectingGfxDetail = fonts::FreeSans9pt7b;

struct TextLine {
  const char* text;
  float vlw_size;
  const lgfx::GFXfont* gfx_font;
  uint16_t color;
};

int lineHeightGfx(const lgfx::GFXfont* font) {
  displayFontSetBitmap(tft, font);
  return tft.fontHeight();
}

int lineHeightVlw(float size) {
  displayFontSetSmoothSize(tft, size);
  return tft.fontHeight();
}

void applyLineStyle(const TextLine& line) {
  if (displayFontIsSmooth()) {
    displayFontSetSmoothSize(tft, line.vlw_size);
  } else {
    displayFontSetBitmap(tft, line.gfx_font);
  }
}

void drawTextBlock(uint16_t bg, const TextLine* lines, size_t count) {
  tft.fillScreen(bg);
  tft.setTextDatum(textdatum_t::middle_center);

  int total_h = 0;
  for (size_t i = 0; i < count; ++i) {
    if (displayFontIsSmooth()) {
      total_h += lineHeightVlw(lines[i].vlw_size);
    } else {
      total_h += lineHeightGfx(lines[i].gfx_font);
    }
    if (i + 1 < count) {
      total_h += kLineGap;
    }
  }

  int y = (config::kDisplayHeight - total_h) / 2;
  for (size_t i = 0; i < count; ++i) {
    applyLineStyle(lines[i]);
    tft.setTextColor(lines[i].color, bg);
    const int h =
        displayFontIsSmooth() ? lineHeightVlw(lines[i].vlw_size)
                              : lineHeightGfx(lines[i].gfx_font);
    tft.drawString(lines[i].text, kCenterX, y + h / 2);
    y += h + kLineGap;
  }
}

void drawStatusAppVersion() {
  tft.setTextDatum(textdatum_t::bottom_right);
  tft.setTextColor(hex2rgb565(COLOR_OVERLAY), hex2rgb565(COLOR_BASE));
  if (displayFontIsSmooth()) {
    displayFontSetSmoothSize(tft, 0.60f);
  } else {
    displayFontSetBitmap(tft, &kConnectingGfxDetail);
  }
  tft.drawString(config::kAppVersion, config::kDisplayWidth - 2, config::kDisplayHeight - 2);
}

constexpr float kConnectingDetailVlw = 0.92f;

void applyConnectingDetailStyle() {
  if (displayFontIsSmooth()) {
    displayFontSetSmoothSize(tft, kConnectingDetailVlw);
  } else {
    displayFontSetBitmap(tft, &kConnectingGfxDetail);
  }
}

/** SSID on one line; truncate with … if wider than kConnectingTextMaxWidthPx. */
void fitSsidLine() {
  strncpy(s_ssid_line, s_connecting_ssid, sizeof(s_ssid_line) - 1);
  s_ssid_line[sizeof(s_ssid_line) - 1] = '\0';
  applyConnectingDetailStyle();
  if (tft.textWidth(s_ssid_line) <= kConnectingTextMaxWidthPx) {
    return;
  }
  const size_t len = strlen(s_connecting_ssid);
  for (size_t n = len; n > 0; --n) {
    snprintf(s_ssid_line, sizeof(s_ssid_line), "%.*s…", static_cast<int>(n),
             s_connecting_ssid);
    if (tft.textWidth(s_ssid_line) <= kConnectingTextMaxWidthPx) {
      return;
    }
  }
  strncpy(s_ssid_line, "…", sizeof(s_ssid_line) - 1);
  s_ssid_line[sizeof(s_ssid_line) - 1] = '\0';
}

void drawConnectingText() {
  tft.fillScreen(hex2rgb565(COLOR_BASE));

  tft.setTextDatum(textdatum_t::middle_center);
  tft.setTextColor(hex2rgb565(COLOR_TEXT), hex2rgb565(COLOR_BASE));

  applyConnectingDetailStyle();
  const int detail_h = tft.fontHeight();
  const int total_h = detail_h * 2 + kLineGap;
  const int block_top = (config::kDisplayHeight - total_h) / 2;
  constexpr int kPanelPadY = 8;
  tft.fillRect(kCenterX - kConnectingTextMaxWidthPx / 2, block_top - kPanelPadY,
               kConnectingTextMaxWidthPx, total_h + kPanelPadY * 2, hex2rgb565(COLOR_BASE));

  int y = block_top;
  tft.drawString("Connecting to", kCenterX, y + detail_h / 2);
  y += detail_h + kLineGap;
  tft.drawString(s_ssid_line, kCenterX, y + detail_h / 2);

  drawStatusAppVersion();

  s_connecting_text_drawn = true;
}

void eraseSpinnerDots() {
  for (int i = 0; i < kSpinnerDotCount; ++i) {
    if (!s_spinner_dots[i].drawn) {
      continue;
    }
    tft.fillCircle(s_spinner_dots[i].x, s_spinner_dots[i].y, kSpinnerEraseRadius,
                   hex2rgb565(COLOR_BASE));
    s_spinner_dots[i].drawn = false;
  }
}

void drawSpinnerDots() {
  constexpr float kDegToRad = 0.01745329252f;
  const float head_rad = s_spinner_angle_deg * kDegToRad;

  for (int i = 0; i < kSpinnerDotCount; ++i) {
    const float a = head_rad - static_cast<float>(i) * (6.283185307f / kSpinnerDotCount);
    const int x = kCenterX + static_cast<int>(std::lround(std::cos(a) * kSpinnerRadius));
    const int y = kCenterY + static_cast<int>(std::lround(std::sin(a) * kSpinnerRadius));

    const int fade = 255 - i * 22;
    uint32_t theme_green = COLOR_GREEN;
    uint8_t green_r = (theme_green >> 16) & 0xFF;
    uint8_t green_g = (theme_green >> 8) & 0xFF;
    uint8_t green_b = theme_green & 0xFF;

    const uint8_t r = (green_r * fade) / 255;
    const uint8_t g = (green_g * fade) / 255;
    const uint8_t b = (green_b * fade) / 255;
    
    uint16_t color;
    if (config::kDisplayRgbOrder) {
        color = tft.color565(b, g, r);
    } else {
        color = tft.color565(r, g, b);
    }
    
    tft.fillSmoothCircle(x, y, kSpinnerDotRadius, color);

    s_spinner_dots[i].x = x;
    s_spinner_dots[i].y = y;
    s_spinner_dots[i].drawn = true;
  }
}

}  // namespace

void statusScreenConnectingBegin(const char* ssid) {
  const char* name = (ssid != nullptr && ssid[0] != '\0') ? ssid : "network";
  strncpy(s_connecting_ssid, name, sizeof(s_connecting_ssid) - 1);
  s_connecting_ssid[sizeof(s_connecting_ssid) - 1] = '\0';
  fitSsidLine();
  s_spinner_angle_deg = -90.0f;
  for (auto& dot : s_spinner_dots) {
    dot.drawn = false;
  }
  s_connecting_text_drawn = false;
  drawConnectingText();
  drawSpinnerDots();
}

void statusScreenConnectingTick() {
  if (!s_connecting_text_drawn) {
    drawConnectingText();
  }
  eraseSpinnerDots();
  s_spinner_angle_deg += kSpinnerStepDeg;
  if (s_spinner_angle_deg >= 270.0f) {
    s_spinner_angle_deg -= 360.0f;
  }
  drawSpinnerDots();
}

void statusScreenPortal() {
  const TextLine lines[] = {
      {"Wi-Fi setup", 1.15f, &kPortalGfxTitle, hex2rgb565(COLOR_MAUVE)},
      {"1. Join network:", 1.05f, &kPortalGfxBody, hex2rgb565(COLOR_TEXT)},
      {config::kPortalApName, 1.12f, &kPortalGfxEmphasis, hex2rgb565(COLOR_BLUE)},
      {"2. Open in browser:", 1.05f, &kPortalGfxBody, hex2rgb565(COLOR_TEXT)},
      {config::kPortalHostUrl, 1.12f, &kPortalGfxEmphasis, hex2rgb565(COLOR_GREEN)},
      {"or 192.168.4.1", 1.0f, &kPortalGfxBody, hex2rgb565(COLOR_GREEN)},
  };
  drawTextBlock(hex2rgb565(COLOR_BASE), lines,
                sizeof(lines) / sizeof(lines[0]));
  drawStatusAppVersion();
}

void statusScreenConnectFailed() {
  const TextLine lines[] = {
      {"Could not connect", 1.15f, &kGfxTitle, hex2rgb565(COLOR_YELLOW)},
      {"Check Wi-Fi password", 1.0f, &kGfxBody, hex2rgb565(COLOR_TEXT)},
      {"and signal strength.", 1.0f, &kGfxBody, hex2rgb565(COLOR_TEXT)},
      {"Hold BOOT 3 sec", 1.0f, &kGfxBody, hex2rgb565(COLOR_TEXT)},
      {"to reset Wi-Fi", 1.0f, &kGfxBody, hex2rgb565(COLOR_TEXT)},
  };
  drawTextBlock(hex2rgb565(COLOR_BASE), lines,
                sizeof(lines) / sizeof(lines[0]));
  drawStatusAppVersion();
}

void statusScreenWifiReset() {
  const TextLine lines[] = {
      {"Wi-Fi reset", 1.15f, &kPortalGfxTitle, hex2rgb565(COLOR_YELLOW)},
      {"Restarting...", 1.05f, &kPortalGfxBody, hex2rgb565(COLOR_TEXT)},
  };
  drawTextBlock(hex2rgb565(COLOR_BASE), lines,
                sizeof(lines) / sizeof(lines[0]));
  drawStatusAppVersion();
}
