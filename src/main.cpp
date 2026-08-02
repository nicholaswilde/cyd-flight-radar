/**
 * Plane Radar — WiFi setup, then radar UI on the round GC9A01 display.
 */

#include <Arduino.h>
#include <WiFi.h>
#include <time.h>

#include "config.h"
#include "hardware/display.h"
#include "services/adsb_client.h"
#include "services/radar_location.h"
#include "services/wifi_manager.h"
#include "services/button_manager.h"
#include "secrets.h"
#include "ui/radar_display.h"
#include "ui/radar_range.h"
#include "ui/status_screens.h"
#include "ui/settings_menu.h"

namespace {

bool g_radar_visible = false;
bool g_screen_on = true;
unsigned long g_last_adsb_fetch_ms = 0;

WifiManager wifiManager(WIFI_SSID, WIFI_PASSWORD);
ButtonManager bootButton(0);
WifiState g_last_wifi_state = WIFI_STATE_DISCONNECTED;

void showRadarIfConnected() {
  if (WiFi.status() != WL_CONNECTED) {
    g_radar_visible = false;
    return;
  }
  ui::radarDisplayDraw();
  g_radar_visible = true;
}

void updateBrightness() {
  if (!g_screen_on) return;

  static uint8_t s_current_brightness = 255;
  uint8_t target_brightness = 255;

  if (ui::settings::isAutoDimmingEnabled()) {
    static unsigned long s_last_time_check = 0;
    static struct tm s_timeinfo;
    static bool s_time_valid = false;

    if (millis() - s_last_time_check > 5000) {
      s_last_time_check = millis();
      s_time_valid = getLocalTime(&s_timeinfo, 0);
    }

    if (s_time_valid) {
      if (s_timeinfo.tm_hour >= 20 || s_timeinfo.tm_hour < 6) {
        target_brightness = 100; // Dim level
      }
    }
  }

  if (s_current_brightness != target_brightness) {
    s_current_brightness = target_brightness;
    tft.setBrightness(s_current_brightness);
  }
}

void onRangeTap() {
  ui::radar::rangeNext();
  char range_label[12];
  ui::radar::formatCurrentRing3Label(range_label, sizeof(range_label));
  Serial.printf("Range: %s (outer ~%.0f km)\n", range_label,
                ui::radar::rangeCurrent().outer_km);

  if (g_radar_visible && wifiManager.getState() == WIFI_STATE_CONNECTED) {
    ui::radarDisplayDraw();
    g_last_adsb_fetch_ms = 0; // Force immediate ADSB fetch on next loop
  }
}

void handleInput() {
  ButtonAction action = bootButton.update(millis());
  if (action == ButtonAction::SINGLE_PRESS) {
    if (!g_screen_on) {
      g_screen_on = true;
      tft.setBrightness(255);
    } else {
      onRangeTap();
    }
  } else if (action == ButtonAction::LONG_PRESS) {
    if (g_screen_on) {
      ui::settings::show();
    }
  }

  if (!g_screen_on) {
    return; // Ignore touch input while screen is off
  }

  static bool s_was_touched = false;
  static unsigned long s_touch_start_ms = 0;
  static uint16_t s_tap_x = 0;
  static uint16_t s_tap_y = 0;
  uint16_t tx = 0, ty = 0;
  bool is_touched = tft.getTouch(&tx, &ty);
  
  if (is_touched) {
    if (!s_was_touched) {
      s_was_touched = true;
      s_touch_start_ms = millis();
    }
    // Constantly update tap coordinates to avoid garbage data from the first touch frame
    s_tap_x = tx;
    s_tap_y = ty;
  } else if (!is_touched && s_was_touched) {
    s_was_touched = false;
    unsigned long duration = millis() - s_touch_start_ms;
    if (ui::settings::isVisible()) {
       // LVGL handles touch when visible
    } else if (duration > 40 && duration < 600) {
      if (ui::radarDisplayHandleTouch(s_tap_x, s_tap_y)) {
        ui::radarDisplayRefreshAircraft();
      }
    } else if (duration >= 600) {
      ui::settings::show();
    }
  }
}

TaskHandle_t g_fetch_task = nullptr;

void adsbFetchTask(void* pvParameters) {
  const float fetch_km = ui::radar::fetchRadiusKm();
  services::adsb::fetchUpdate(services::location::lat(),
                              services::location::lon(), fetch_km);
  g_fetch_task = nullptr;
  vTaskDelete(NULL);
}

void triggerFetch() {
  if (g_fetch_task == nullptr) {
    xTaskCreatePinnedToCore(
        adsbFetchTask,      // Function to implement the task
        "ADSB_Fetch",       // Name of the task
        16384,              // Stack size in words (bytes on ESP32)
        NULL,               // Task input parameter
        1,                  // Priority of the task
        &g_fetch_task,      // Task handle
        0);                 // Core where the task should run (0 = network, 1 = UI)
  }
}

}  // namespace

#include <nvs_flash.h>

void setup() {
  Serial.begin(115200);
  delay(500);
  
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    nvs_flash_erase();
    err = nvs_flash_init();
  }

  bootButton.begin();
  displayInit();
  
  services::location::init();
  ui::radar::rangeInit();

  ui::settings::setup();

  wifiManager.begin();
  if (wifiManager.getState() == WIFI_STATE_CONNECTING || wifiManager.getState() == WIFI_STATE_DISCONNECTED) {
    statusScreenConnectingBegin(WIFI_SSID);
  }
}

void loop() {
  handleInput();
  wifiManager.update();
  ui::settings::loop();
  updateBrightness();

  WifiState current_state = wifiManager.getState();
  
  if (ui::settings::isTimezoneChanged()) {
    ui::settings::clearTimezoneChanged();
    configTzTime(ui::settings::getTimezoneStr(), config::kNtpServer);
  }

  if (current_state != g_last_wifi_state) {
    if (current_state == WIFI_STATE_CONNECTED) {
      configTzTime(ui::settings::getTimezoneStr(), config::kNtpServer);
      showRadarIfConnected();
    } else if (current_state == WIFI_STATE_AP_MODE) {
      // In AP Mode
      if (g_radar_visible) {
        g_radar_visible = false;
      }
      statusScreenPortal();
    } else if (current_state == WIFI_STATE_CONNECTING || current_state == WIFI_STATE_DISCONNECTED) {
      if (g_radar_visible) {
        g_radar_visible = false;
      }
      statusScreenConnectingBegin(WIFI_SSID);
    }
    g_last_wifi_state = current_state;
  }

  if (current_state == WIFI_STATE_CONNECTING) {
    statusScreenConnectingTick();
  } else if (current_state == WIFI_STATE_CONNECTED) {
    if (!g_radar_visible) {
      showRadarIfConnected();
    } else {
      if (!ui::settings::isVisible() && g_screen_on) {
        static unsigned long s_last_sweep_ms = 0;
        if (ui::settings::isRadarSweepEnabled() && millis() - s_last_sweep_ms >= 50) { // 20 FPS
          s_last_sweep_ms = millis();
          ui::radarDisplayUpdateAnimation();
        }
      }
      
      if (millis() - g_last_adsb_fetch_ms >= config::kAdsbFetchIntervalMs) {
        g_last_adsb_fetch_ms = millis();
        triggerFetch();
      }
    }
  }

  delay(10);
}
