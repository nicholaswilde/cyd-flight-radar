#pragma once

#include <cstdint>
#include <driver/gpio.h>

namespace config {

// --- Wi-Fi portal ---
constexpr char kPortalApName[] = "PlaneRadar-Setup";
constexpr char kPortalIp[] = "192.168.4.1";
/** mDNS host (no ".local" suffix); browser: http://plane-radar.local */
constexpr char kPortalHostname[] = "plane-radar";
constexpr char kPortalHostUrl[] = "plane-radar.local";

/** Per-attempt STA connect wait (ms); retried kWifiConnectAttempts times. */
constexpr unsigned long kWifiConnectAttemptMs = 15000;
constexpr uint8_t kWifiConnectAttempts = 3;
constexpr unsigned long kWifiPortalTimeoutSec = 0;  // 0 = no timeout while configuring
constexpr unsigned long kWifiConnectingFrameMs = 50;
/** Wait after disconnect before reconnecting (avoids portal on brief drops). */
constexpr unsigned long kWifiDownGraceMs = 4000;
/** Minimum interval between background reconnect tries. */
constexpr unsigned long kWifiReconnectIntervalMs = 15000;

// --- BOOT button (ESP32-C3 Super Mini, active LOW) ---
constexpr gpio_num_t kBootPin = GPIO_NUM_9;
constexpr unsigned long kBootResetHoldMs = 3000UL;

/** Ignore BOOT taps shorter than this (debounce). */
constexpr unsigned long kBootTapMinMs = 40UL;

// --- Display: GC9A01 1.28" round 240×240 (SPI) ---
constexpr gpio_num_t kDisplayPinRst = GPIO_NUM_0;
constexpr gpio_num_t kDisplayPinCs = GPIO_NUM_1;
constexpr gpio_num_t kDisplayPinDc = GPIO_NUM_10;
constexpr gpio_num_t kDisplayPinMosi = GPIO_NUM_3;  // display SDA
constexpr gpio_num_t kDisplayPinSclk = GPIO_NUM_4;  // display SCL

#ifndef TFT_WIDTH
constexpr int kDisplayWidth = 240;
constexpr int kDisplayHeight = 240;
#else
constexpr int kDisplayWidth = TFT_WIDTH;
constexpr int kDisplayHeight = TFT_HEIGHT;
#endif

constexpr uint32_t kDisplaySpiWriteHz = 40000000;
// GC9A01 modules often need invert + BGR for correct black/green output
constexpr bool kDisplayInvert = true;
constexpr bool kDisplayRgbOrder = true;

// --- Radar center defaults (overridden via WiFi setup portal) ---
constexpr double kDefaultRadarLat = 52.3676;
constexpr double kDefaultRadarLon = 4.9041;

/** Poll adsb.fi (API public limit: 1 req/s). */
constexpr unsigned long kAdsbFetchIntervalMs = 3000;
/** Legacy scale unused — fetch uses radar::fetchRadiusKm() to screen edge. */
constexpr float kAdsbFetchRadiusScale = 1.0f;
/** false = hide aircraft with alt_baro "ground"; true = show them too. */
constexpr bool kAdsbShowGroundAircraft = false;

// --- UI colors (RGB565) — status screens (Catppuccin Mocha) ---
constexpr uint16_t kColorBlack = 0x18E5;  // Base (#1e1e2e)
constexpr uint16_t kColorYellow = 0xFF15; // Yellow (#f9e2af)
constexpr uint16_t kColorMauve = 0xCD3E;  // Mauve (#cba6f7)
constexpr uint16_t kColorGreen = 0xA714;  // Green (#a6e3a1)
constexpr uint16_t kColorBlue = 0x8DBF;   // Blue (#89b4fa)
constexpr uint16_t kTextOnYellow = 0x18E5; // Base (#1e1e2e)
constexpr uint16_t kTextOnBlack = 0xCEBE; // Text (#cdd6f4)

constexpr char kAppVersion[] = "v0.1.0";

// --- NTP and Timezone Settings ---
// Find your region's POSIX string here: https://gist.github.com/alwynallan/24d96091655391107939
constexpr char kNtpServer[] = "pool.ntp.org";
constexpr char kTimezoneDefault[] = "UTC0"; // Default: UTC

// --- Theme Settings ---
#define COLOR_BASE         (getCatppuccinFlavor(ui::settings::getThemeFlavor()).base)
#define COLOR_MANTLE       (getCatppuccinFlavor(ui::settings::getThemeFlavor()).mantle)
#define COLOR_CRUST        (getCatppuccinFlavor(ui::settings::getThemeFlavor()).crust)
#define COLOR_TEXT         (getCatppuccinFlavor(ui::settings::getThemeFlavor()).text)
#define COLOR_OVERLAY      (getCatppuccinFlavor(ui::settings::getThemeFlavor()).overlay)
#define COLOR_BLUE         (getCatppuccinFlavor(ui::settings::getThemeFlavor()).blue)
#define COLOR_GREEN        (getCatppuccinFlavor(ui::settings::getThemeFlavor()).green)
#define COLOR_RED          (getCatppuccinFlavor(ui::settings::getThemeFlavor()).red)
#define COLOR_YELLOW       (getCatppuccinFlavor(ui::settings::getThemeFlavor()).yellow)
#define COLOR_PEACH        (getCatppuccinFlavor(ui::settings::getThemeFlavor()).peach)
#define COLOR_MAUVE        (getCatppuccinFlavor(ui::settings::getThemeFlavor()).mauve)
#define COLOR_LAVENDER     (getCatppuccinFlavor(ui::settings::getThemeFlavor()).lavender)
#define COLOR_HEADER_TEXT  (getCatppuccinFlavor(ui::settings::getThemeFlavor()).header_text)

// --- Radar Sweep Animation ---
constexpr bool kRadarSweepEnabled = true;
constexpr float kRadarSweepDurationMs = 6000.0f; // Time (ms) for one full rotation

}  // namespace config
