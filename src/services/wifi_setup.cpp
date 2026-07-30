#include "services/wifi_setup.h"

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ImprovWiFiLibrary.h>
#include <Preferences.h>
#include <esp_system.h>
#include <esp_wifi.h>

#ifdef WM_MDNS
#include <ESPmDNS.h>
#endif

#include "config.h"
#include "services/radar_location.h"
#include "ui/radar_range.h"
#include "ui/status_screens.h"

#if __has_include("secrets.h")
#include "secrets.h"
#endif

#ifndef WIFI_SSID
#define WIFI_SSID ""
#define WIFI_PASSWORD ""
#endif

portMUX_TYPE s_boot_mux = portMUX_INITIALIZER_UNLOCKED;
volatile bool s_boot_tap_pending = false;
volatile bool s_boot_is_down = false;
volatile unsigned long s_boot_down_ms = 0;
bool s_long_press_handled = false;
bool s_boot_interrupt_attached = false;

void IRAM_ATTR onBootButtonIsr() {
  const bool down = digitalRead(config::kBootPin) == LOW;
  const unsigned long now = millis();
  portENTER_CRITICAL_ISR(&s_boot_mux);
  if (down) {
    s_boot_is_down = true;
    s_boot_down_ms = now;
  } else if (s_boot_is_down) {
    const unsigned long held = now - s_boot_down_ms;
    if (held >= config::kBootTapMinMs && held < config::kBootResetHoldMs) {
      s_boot_tap_pending = true;
    }
    s_boot_is_down = false;
  }
  portEXIT_CRITICAL_ISR(&s_boot_mux);
}

void initBootButton() {
  pinMode(config::kBootPin, INPUT_PULLUP);
  if (s_boot_interrupt_attached) {
    return;
  }
  attachInterrupt(digitalPinToInterrupt(static_cast<uint8_t>(config::kBootPin)),
                  onBootButtonIsr, CHANGE);
  s_boot_interrupt_attached = true;
}

namespace {

constexpr char kWifiPrefsNamespace[] = "wifi";
constexpr char kPrefsForcePortalKey[] = "portal";
constexpr char kPrefsSsidKey[] = "ssid";
constexpr char kPrefsPassKey[] = "pass";

bool s_force_config_portal = false;

WebServer* s_webServer = nullptr;
DNSServer* s_dnsServer = nullptr;
ImprovWiFi* s_improv = nullptr;
String s_cachedNetworksHTML = "";
bool s_ap_mode_active = false;

void stopLanWebPortal();
bool wifiLinkUp();

String getAPSSID() {
    String mac = WiFi.macAddress();
    String cleanMac = "";
    for (size_t i = 0; i < mac.length(); i++) {
        if (mac[i] != ':') {
            cleanMac += mac[i];
        }
    }
    String suffix = (cleanMac.length() >= 4) ? String(cleanMac.c_str() + cleanMac.length() - 4) : "ESP32";
    for (size_t i = 0; i < suffix.length(); i++) {
        suffix[i] = toupper(suffix[i]);
    }
    return String(config::kPortalApName) + "-" + suffix;
}

void markForceConfigPortal() {
  s_force_config_portal = true;
  Preferences prefs;
  if (!prefs.begin(kWifiPrefsNamespace, false)) return;
  prefs.putBool(kPrefsForcePortalKey, true);
  prefs.end();
}

bool consumeForceConfigPortal() {
  if (s_force_config_portal) {
    s_force_config_portal = false;
    Preferences prefs;
    if (prefs.begin(kWifiPrefsNamespace, false)) {
      prefs.remove(kPrefsForcePortalKey);
      prefs.end();
    }
    return true;
  }

  Preferences prefs;
  if (!prefs.begin(kWifiPrefsNamespace, true)) return false;
  const bool pending = prefs.getBool(kPrefsForcePortalKey, false);
  prefs.end();
  if (!pending) return false;

  if (prefs.begin(kWifiPrefsNamespace, false)) {
    prefs.remove(kPrefsForcePortalKey);
    prefs.end();
  }
  return true;
}

bool storedWifiCredentials() {
  Preferences prefs;
  if (!prefs.begin(kWifiPrefsNamespace, true)) return false;
  String ssid = prefs.getString(kPrefsSsidKey, "");
  prefs.end();
  return ssid.length() > 0;
}

String getStoredSSID() {
  Preferences prefs;
  if (!prefs.begin(kWifiPrefsNamespace, true)) return "";
  String ssid = prefs.getString(kPrefsSsidKey, "");
  prefs.end();
  return ssid;
}

String getStoredPass() {
  Preferences prefs;
  if (!prefs.begin(kWifiPrefsNamespace, true)) return "";
  String pass = prefs.getString(kPrefsPassKey, "");
  prefs.end();
  return pass;
}

void saveWifiCredentials(const String& ssid, const String& pass) {
  Preferences prefs;
  if (prefs.begin(kWifiPrefsNamespace, false)) {
    prefs.putString(kPrefsSsidKey, ssid);
    prefs.putString(kPrefsPassKey, pass);
    prefs.end();
  }
}

void eraseWifiCredentials() {
  stopLanWebPortal();
  WiFi.setAutoReconnect(false);
  WiFi.mode(WIFI_OFF);
  delay(100);

  Preferences prefs;
  if (prefs.begin(kWifiPrefsNamespace, false)) {
    prefs.remove(kPrefsSsidKey);
    prefs.remove(kPrefsPassKey);
    prefs.end();
  }

  WiFi.disconnect(true, true);
  delay(100);
}

void resetWifiCredentials() {
  markForceConfigPortal();
  eraseWifiCredentials();
  services::location::clear();
  ui::radar::unitsReset();
  Serial.println("WiFi credentials, location, and units cleared");
}

bool wifiLinkUp() {
  return WiFi.status() == WL_CONNECTED &&
         WiFi.localIP() != IPAddress(0, 0, 0, 0);
}

void handleRoot() {
    int16_t scanStatus = WiFi.scanComplete();
    if (scanStatus >= 0) {
        s_cachedNetworksHTML = "";
        for (int i = 0; i < scanStatus; ++i) {
            String ssidName = WiFi.SSID(i);
            int32_t rssi = WiFi.RSSI(i);
            s_cachedNetworksHTML += "<div class='net-item' onclick='selectSSID(\"" + ssidName + "\")'>";
            s_cachedNetworksHTML += "<span>" + ssidName + "</span>";
            s_cachedNetworksHTML += "<span style='color: #a6adc8; font-size: 12px;'>" + String(rssi) + " dBm</span>";
            s_cachedNetworksHTML += "</div>";
        }
        WiFi.scanDelete();
    } else if (scanStatus == WIFI_SCAN_FAILED) {
        WiFi.scanNetworks(true, false, false, 150);
        if (s_cachedNetworksHTML.length() == 0 || s_cachedNetworksHTML.indexOf("Scanning in progress") != -1) {
            s_cachedNetworksHTML = "<div class='net-item' style='color: #a6adc8;'>Scanning in progress... Please refresh.</div>";
        }
    }

    String html = "<!DOCTYPE html><html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
    if (scanStatus == WIFI_SCAN_RUNNING || scanStatus == WIFI_SCAN_FAILED) {
        html += "<meta http-equiv='refresh' content='3'>";
    }
    html += "<title>CYD Flight Radar Setup</title>";
    html += "<style>";
    html += "body { font-family: 'Inter', system-ui, sans-serif; background: #1e1e2e; color: #cdd6f4; margin: 0; padding: 20px; display: flex; justify-content: center; align-items: center; min-height: 100vh; box-sizing: border-box; }";
    html += ".card { background: #181825; border-radius: 12px; padding: 30px; width: 100%; max-width: 400px; box-shadow: 0 8px 30px rgba(0,0,0,0.3); border: 1px solid #313244; }";
    html += "h2 { color: #f5c2e7; margin-top: 0; margin-bottom: 20px; font-weight: 600; text-align: center; }";
    html += "label { display: block; margin-bottom: 8px; color: #a6adc8; font-size: 14px; }";
    html += "input[type='text'], input[type='password'], input[type='number'] { width: 100%; padding: 12px; margin-bottom: 20px; border-radius: 6px; border: 1px solid #45475a; background: #313244; color: #cdd6f4; font-size: 16px; box-sizing: border-box; }";
    html += "input:focus { outline: none; border-color: #f5c2e7; }";
    html += "input[type='checkbox'] { transform: scale(1.5); margin-right: 10px; accent-color: #cba6f7; }";
    html += ".checkbox-group { display: flex; align-items: center; margin-bottom: 20px; }";
    html += ".checkbox-group label { margin-bottom: 0; color: #cdd6f4; }";
    html += "button { width: 100%; padding: 12px; background: #cba6f7; border: none; border-radius: 6px; color: #11111b; font-size: 16px; font-weight: bold; cursor: pointer; transition: background 0.2s; }";
    html += "button:hover { background: #f5c2e7; }";
    html += ".net-list { margin-bottom: 20px; max-height: 150px; overflow-y: auto; border: 1px solid #313244; border-radius: 6px; padding: 10px; background: #11111b; }";
    html += ".net-item { display: flex; justify-content: space-between; padding: 8px; cursor: pointer; border-bottom: 1px solid #1e1e2e; }";
    html += ".net-item:last-child { border-bottom: none; }";
    html += ".net-item:hover { background: #313244; color: #f5c2e7; }";
    html += "</style>";
    html += "<script>";
    html += "function selectSSID(ssid) { document.getElementById('ssid').value = ssid; }";
    html += "</script>";
    html += "</head><body>";
    html += "<div class='card'>";
    html += "<h2>Device Configuration</h2>";
    html += "<form method='POST' action='/save'>";
    
    html += "<div style='display: flex; justify-content: space-between; align-items: center;'>";
    html += "<label style='margin-bottom: 0;'>Select Network</label>";
    html += "<a href='/scan' style='color: #cba6f7; font-size: 12px; text-decoration: none;'>\xE2\x86\xBA Refresh</a>";
    html += "</div>";
    html += "<div style='height: 8px;'></div>";
    
    html += "<div class='net-list'>";
    html += s_cachedNetworksHTML;
    html += "</div>";
    
    html += "<label for='ssid'>SSID</label>";
    html += "<input type='text' id='ssid' name='ssid' placeholder='SSID name' value='" + getStoredSSID() + "'>";
    
    html += "<label for='pass'>Password</label>";
    html += "<input type='password' id='pass' name='pass' placeholder='Password'>";
    
    html += "<label for='lat'>Latitude (deg)</label>";
    html += "<input type='number' step='0.000001' id='lat' name='lat' value='" + String(services::location::lat(), 6) + "'>";
    
    html += "<label for='lon'>Longitude (deg)</label>";
    html += "<input type='number' step='0.000001' id='lon' name='lon' value='" + String(services::location::lon(), 6) + "'>";
    
    html += "<div class='checkbox-group'>";
    html += "<input type='checkbox' id='miles' name='miles' " + String(ui::radar::useMiles() ? "checked" : "") + ">";
    html += "<label for='miles'>Display distances in miles</label>";
    html += "</div>";
    
    html += "<div class='checkbox-group'>";
    html += "<input type='checkbox' id='runways' name='runways' " + String(ui::radar::showRunways() ? "checked" : "") + ">";
    html += "<label for='runways'>Show airport runways</label>";
    html += "</div>";
    
    html += "<button type='submit'>Save & Apply</button>";
    html += "</form>";
    html += "</div>";
    html += "</body></html>";

    s_webServer->send(200, "text/html", html);
}

void handleSave() {
    String ssid = s_webServer->arg("ssid");
    String pass = s_webServer->arg("pass");
    String lat = s_webServer->arg("lat");
    String lon = s_webServer->arg("lon");
    bool useMiles = s_webServer->hasArg("miles");
    bool showRunways = s_webServer->hasArg("runways");

    Serial.printf("[WiFi] Saved new configuration via portal.\n");

    String html = "<!DOCTYPE html><html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
    html += "<title>Credentials Saved</title>";
    html += "<style>";
    html += "body { font-family: 'Inter', system-ui, sans-serif; background: #1e1e2e; color: #cdd6f4; margin: 0; padding: 20px; display: flex; justify-content: center; align-items: center; min-height: 100vh; box-sizing: border-box; }";
    html += ".card { background: #181825; border-radius: 12px; padding: 30px; width: 100%; max-width: 400px; box-shadow: 0 8px 30px rgba(0,0,0,0.3); border: 1px solid #313244; text-align: center; }";
    html += "h2 { color: #a6e3a1; margin-top: 0; margin-bottom: 20px; }";
    html += "p { color: #cdd6f4; margin-bottom: 20px; line-height: 1.5; }";
    html += "</style></head><body>";
    html += "<div class='card'>";
    html += "<h2>Configuration Saved</h2>";
    html += "<p>Applying settings...</p>";
    html += "<p>The device will now attempt to connect. You can close this page.</p>";
    html += "</div>";
    html += "</body></html>";

    s_webServer->send(200, "text/html", html);
    delay(1000);

    if (ssid.length() > 0) {
        saveWifiCredentials(ssid, pass);
    }
    
    if (!services::location::saveFromStrings(lat.c_str(), lon.c_str())) {
        Serial.println("Invalid lat/lon in portal — keeping previous location");
    }
    ui::radar::saveMilesFromPortal(useMiles ? "T" : "F");
    ui::radar::saveRunwaysFromPortal(showRunways ? "T" : "F");

    ESP.restart();
}

void handleNotFound() {
    if (s_ap_mode_active) {
        s_webServer->sendHeader("Location", "http://192.168.4.1/", true);
        s_webServer->send(302, "text/plain", "");
    } else {
        s_webServer->send(404, "text/plain", "Not Found");
    }
}

void setupWebServer() {
    if (s_webServer) return;
    s_webServer = new WebServer(80);
    s_webServer->on("/", handleRoot);
    s_webServer->on("/save", handleSave);
    s_webServer->on("/scan", []() {
        WiFi.scanNetworks(true, false, false, 150);
        String html = "<!DOCTYPE html><html><head>";
        html += "<meta http-equiv=\"refresh\" content=\"3;url=/\">";
        html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
        html += "<title>Scanning...</title>";
        html += "<style>";
        html += "body { font-family: 'Inter', system-ui, sans-serif; background: #1e1e2e; color: #cdd6f4; margin: 0; padding: 20px; display: flex; justify-content: center; align-items: center; min-height: 100vh; }";
        html += ".card { background: #181825; border-radius: 12px; padding: 30px; width: 100%; max-width: 400px; box-shadow: 0 8px 30px rgba(0,0,0,0.3); border: 1px solid #313244; text-align: center; }";
        html += "h2 { color: #f5c2e7; margin-top: 0; }";
        html += "p { color: #a6adc8; }";
        html += "</style></head><body>";
        html += "<div class='card'><h2>Scanning for Wi-Fi...</h2><p>Please wait while we refresh the network list.</p></div>";
        html += "</body></html>";
        s_webServer->send(200, "text/html", html);
    });
    s_webServer->onNotFound(handleNotFound);
    s_webServer->begin();
}

void startLanWebPortal() {
  if (!wifiLinkUp() || s_webServer != nullptr) {
    return;
  }
  setupWebServer();
#ifdef WM_MDNS
  MDNS.end();
  if (MDNS.begin(config::kPortalHostname)) {
    MDNS.addService("http", "tcp", 80);
  }
#endif
  Serial.printf("LAN config: http://%s.local or http://%s\n",
                config::kPortalHostname, WiFi.localIP().toString().c_str());
}

void stopLanWebPortal() {
  if (s_webServer) {
    s_webServer->stop();
    delete s_webServer;
    s_webServer = nullptr;
  }
  if (s_dnsServer) {
    s_dnsServer->stop();
    delete s_dnsServer;
    s_dnsServer = nullptr;
  }
#ifdef WM_MDNS
  MDNS.end();
#endif
  s_ap_mode_active = false;
}

void prepareSta() {
  WiFi.setTxPower(WIFI_POWER_8_5dBm);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(WIFI_PS_NONE);
  WiFi.setAutoReconnect(true);
}

void startStaConnect(const String& ssid, const String& pass) {
  prepareSta();
  if (ssid.length() > 0) {
    WiFi.begin(ssid.c_str(), pass.c_str());
  } else {
    WiFi.begin();
  }
}

bool waitForLinkWithUi(const char* ssid_for_ui, unsigned long attempt_ms) {
  const unsigned long deadline = millis() + attempt_ms;
  while (millis() < deadline) {
    if (wifiLinkUp()) {
      return true;
    }
    bootButtonPollLongPress();
    statusScreenConnectingTick();
    delay(config::kWifiConnectingFrameMs);
  }
  return wifiLinkUp();
}

bool tryConnectWithUi(const String& ssid, const String& pass, bool show_ui) {
  if (wifiLinkUp()) {
    return true;
  }

  const char* ui_ssid = ssid.length() > 0 ? ssid.c_str() : "network";
  if (show_ui) {
    statusScreenConnectingBegin(ui_ssid);
  }

  for (uint8_t attempt = 1; attempt <= config::kWifiConnectAttempts; ++attempt) {
    if (attempt > 1) {
      Serial.printf("WiFi connect retry %u/%u\n", (unsigned)attempt,
                    (unsigned)config::kWifiConnectAttempts);
      WiFi.disconnect(true);
      WiFi.mode(WIFI_OFF);
      delay(400);
    }

    startStaConnect(ssid, pass);

    if (waitForLinkWithUi(ui_ssid, config::kWifiConnectAttemptMs)) {
      return true;
    }
  }

  return false;
}

bool connectSavedNetwork(bool show_ui) {
  if (!storedWifiCredentials()) {
    return false;
  }
  
  const String ssid = getStoredSSID();
  if (ssid.length() == 0) {
    return false;
  }
  const String pass = getStoredPass();
  return tryConnectWithUi(ssid, pass, show_ui);
}

bool openConfigPortal() {
  stopLanWebPortal();
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  delay(50);
  
  statusScreenPortal();
  
  WiFi.mode(WIFI_AP_STA);
  WiFi.setTxPower(WIFI_POWER_11dBm);
  delay(100);
  
  IPAddress apIP(192, 168, 4, 1);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  delay(100);
  
  String apSSID = getAPSSID();
  WiFi.softAP(apSSID.c_str());
  delay(200);

  s_cachedNetworksHTML = "<div class='net-item' style='color: #a6adc8;'>Scanning in progress... Please refresh.</div>";
  s_ap_mode_active = true;

  s_dnsServer = new DNSServer();
  s_dnsServer->setErrorReplyCode(DNSReplyCode::NoError);
  s_dnsServer->start(53, "*", apIP);
  
  setupWebServer();
  
#ifdef WM_MDNS
  if (MDNS.begin(config::kPortalHostname)) {
    MDNS.addService("http", "tcp", 80);
    Serial.printf("Setup portal: http://%s.local (or http://%s)\n",
                  config::kPortalHostname, "192.168.4.1");
  } else {
    Serial.printf("Setup portal: http://%s (mDNS unavailable)\n", "192.168.4.1");
  }
#else
  Serial.printf("Setup portal: http://%s\n", "192.168.4.1");
#endif

  while (true) {
    bootButtonPollLongPress();
    
    if (s_improv) s_improv->handleSerial();
    s_dnsServer->processNextRequest();
    s_webServer->handleClient();
    
    if (wifiLinkUp()) {
        return true;
    }
    
    delay(10);
  }
  
  return false;
}

void setupImprov() {
  if (s_improv) return;
  s_improv = new ImprovWiFi(&Serial);
  s_improv->setDeviceInfo(ImprovTypes::ChipFamily::CF_ESP32, "CYD-Flight-Radar", "1.0", "CYD Flight Radar", "http://{LOCAL_IPV4}");
  s_improv->setCustomConnectWiFi([](const char *ssid, const char *password) {
      Serial.printf("\n[WiFi] Improv connecting to %s...\n", ssid);
      WiFi.softAPdisconnect(true);
      WiFi.mode(WIFI_STA);
      WiFi.disconnect();
      delay(100);
      
      WiFi.begin(ssid, password);
      int attempts = 0;
      while (WiFi.status() != WL_CONNECTED && attempts < 16) { 
          delay(500);
          attempts++;
      }
      return WiFi.status() == WL_CONNECTED;
  });

  s_improv->onImprovConnected([](const char *ssid, const char *password) {
      Serial.printf("\n[WiFi] Improv provisioned successfully!\n");
      saveWifiCredentials(String(ssid), String(password));
  });
}

}  // namespace

bool wifiShowsSetupScreenOnBoot() {
  if (s_force_config_portal) {
    return true;
  }
  Preferences prefs;
  if (!prefs.begin(kWifiPrefsNamespace, true)) {
    return false;
  }
  const bool pending = prefs.getBool(kPrefsForcePortalKey, false);
  prefs.end();
  return pending;
}

bool wifiBootButtonPressed() {
  return digitalRead(config::kBootPin) == LOW;
}

void bootButtonInit() { initBootButton(); }

bool bootButtonConsumeTap() {
  portENTER_CRITICAL(&s_boot_mux);
  const bool tap = s_boot_tap_pending;
  if (tap) {
    s_boot_tap_pending = false;
  }
  portEXIT_CRITICAL(&s_boot_mux);
  return tap;
}

void bootButtonPollLongPress() {
  if (wifiBootButtonPressed()) {
    portENTER_CRITICAL(&s_boot_mux);
    if (!s_boot_is_down) {
      s_boot_is_down = true;
      s_boot_down_ms = millis();
    }
    const unsigned long down_ms = s_boot_down_ms;
    portEXIT_CRITICAL(&s_boot_mux);

    if (!s_long_press_handled &&
        millis() - down_ms >= config::kBootResetHoldMs) {
      s_long_press_handled = true;
      Serial.println("BOOT held — resetting WiFi");
      wifiResetCredentialsAndReboot();
    }
  } else {
    portENTER_CRITICAL(&s_boot_mux);
    s_boot_is_down = false;
    portEXIT_CRITICAL(&s_boot_mux);
    s_long_press_handled = false;
  }
}

void wifiResetCredentialsAndReboot() {
  resetWifiCredentials();
  statusScreenWifiReset();
  delay(800);
  esp_restart();
}

bool wifiReconnect() {
  initBootButton();
  Serial.println("WiFi reconnecting...");
  return connectSavedNetwork(true);
}

void wifiLoop() {
  if (wifiLinkUp()) {
    if (!s_ap_mode_active && s_webServer == nullptr) {
      startLanWebPortal();
    }
    if (s_webServer) {
      bootButtonPollLongPress();
      s_webServer->handleClient();
      if (s_improv) s_improv->handleSerial();
    }
  } else {
    stopLanWebPortal();
  }
}

bool wifiSetupConnect() {
  initBootButton();
  setupImprov();

  const bool force_portal = consumeForceConfigPortal();
  WiFi.setAutoReconnect(false);

  if (force_portal) {
    eraseWifiCredentials();
    WiFi.mode(WIFI_OFF);
    delay(100);
  }

  if (force_portal) {
    Serial.println("Opening WiFi setup portal (after reset)");
    if (openConfigPortal() && wifiLinkUp()) {
      WiFi.setAutoReconnect(true);
      Serial.printf("Connected: %s  IP %s\n", WiFi.SSID().c_str(),
                    WiFi.localIP().toString().c_str());
      return true;
    }
    Serial.println("WiFi connection failed");
    statusScreenConnectFailed();
    return false;
  }

  Serial.println("Connecting to WiFi (portal opens if needed)...");

  if (wifiLinkUp()) {
    WiFi.setAutoReconnect(true);
    Serial.printf("Connected: %s  IP %s\n", WiFi.SSID().c_str(),
                  WiFi.localIP().toString().c_str());
    return true;
  }

  if (String(WIFI_SSID) != "YOUR_WIFI_SSID" && String(WIFI_SSID).length() > 0) {
    Serial.println("Using WiFi credentials from secrets.h");
    if (tryConnectWithUi(WIFI_SSID, WIFI_PASSWORD, true)) {
      WiFi.setAutoReconnect(true);
      Serial.printf("Connected: %s  IP %s\n", WiFi.SSID().c_str(),
                    WiFi.localIP().toString().c_str());
      return true;
    }
    Serial.println("secrets.h WiFi connection failed, falling back...");
  }

  if (storedWifiCredentials() && connectSavedNetwork(true)) {
    WiFi.setAutoReconnect(true);
    Serial.printf("Connected: %s  IP %s\n", WiFi.SSID().c_str(),
                  WiFi.localIP().toString().c_str());
    return true;
  }

  if (storedWifiCredentials()) {
    Serial.println("Saved WiFi could not connect — opening setup portal");
  } else {
    Serial.println("No saved WiFi — opening setup portal");
  }

  if (openConfigPortal() && wifiLinkUp()) {
    WiFi.setAutoReconnect(true);
    Serial.printf("Connected: %s  IP %s\n", WiFi.SSID().c_str(),
                  WiFi.localIP().toString().c_str());
    return true;
  }

  Serial.println("WiFi connection failed");
  statusScreenConnectFailed();
  return false;
}
