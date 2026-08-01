#include "services/wifi_manager.h"
#include <Arduino.h>
#include <Preferences.h>

#include "config.h"
#include "services/radar_location.h"

#ifndef NATIVE_TEST







#include <ArduinoJson.h>
#endif



static void configureStaticIP() {
#ifndef NATIVE_TEST
#ifdef STATIC_IP
    IPAddress local_ip;
    if (local_ip.fromString(STATIC_IP)) {
        IPAddress gateway_ip;
        IPAddress subnet_ip;
        IPAddress dns_ip;

        #ifdef STATIC_GATEWAY
        gateway_ip.fromString(STATIC_GATEWAY);
        #endif
        #ifdef STATIC_SUBNET
        subnet_ip.fromString(STATIC_SUBNET);
        #endif
        #ifdef STATIC_DNS
        dns_ip.fromString(STATIC_DNS);
        #endif

        if (WiFi.config(local_ip, gateway_ip, subnet_ip, dns_ip)) {
            Serial.println("[WiFi] Static IP configured successfully.");
        } else {
            Serial.println("[WiFi] Failed to configure Static IP.");
        }
    }
#endif
#endif
}

WifiManager::WifiManager(const char* ssid, const char* password)
    : _ssid(ssid), _password(password), _state(WIFI_STATE_DISCONNECTED), _lastReconnectAttempt(0), _connectionStartTime(0) {}

void WifiManager::begin() {
    Serial.println("[WiFi] Starting Wi-Fi Manager...");
#ifndef NATIVE_TEST
    WiFi.setAutoReconnect(true);
    WiFi.setTxPower(WIFI_POWER_11dBm);

    _improv = new ImprovWiFi(&Serial);
    _improv->setDeviceInfo(ImprovTypes::ChipFamily::CF_ESP32, "CYD-Weather-Station", "1.0", "CYD Weather Station", "http://{LOCAL_IPV4}");
    
    _improv->setCustomConnectWiFi([](const char *ssid, const char *password) {
        Serial.printf("\n[WiFi] Improv connecting to %s...\n", ssid);
        // Turn off AP mode to speed up STA connection and avoid channel conflicts
        WiFi.softAPdisconnect(true);
        WiFi.mode(WIFI_STA);
        WiFi.disconnect();
        delay(100);
        
        WiFi.begin(ssid, password);
        int attempts = 0;
        // Wait up to 8 seconds (16 * 500ms) to prevent browser RPC timeout (usually 10s)
        while (WiFi.status() != WL_CONNECTED && attempts < 16) { 
            delay(500);
            attempts++;
        }
        return WiFi.status() == WL_CONNECTED;
    });

    _improv->onImprovConnected([](const char *ssid, const char *password) {
        Serial.printf("\n[WiFi] Improv provisioned successfully!\n");
        
        Preferences preferences;
        preferences.begin("wifi", false);
        preferences.putString("ssid", ssid);
        preferences.putString("pass", password);
        preferences.end();
        // No need to restart; WifiManager::update() handles the state transition to WIFI_STATE_CONNECTED
    });
#endif
    WiFi.mode(WIFI_STA);
    
    Preferences preferences;
    preferences.begin("wifi", true);
    String saved_ssid = preferences.getString("ssid", "");
    String saved_pass = preferences.getString("pass", "");
    preferences.end();
    
    if (saved_ssid.length() > 0) {
        _ssid = saved_ssid;
        _password = saved_pass;
    }

    
    if (_ssid.length() == 0) {
        Serial.println("[WiFi] No credentials configured. Launching AP mode directly...");
        startAPMode();
    } else {
        configureStaticIP();
        WiFi.begin(_ssid.c_str(), _password.c_str());
        _state = WIFI_STATE_CONNECTING;
        _connectionStartTime = millis();
        Serial.printf("[WiFi] Connecting to %s...\n", _ssid.c_str());
    }
}

void WifiManager::update() {
#ifndef NATIVE_TEST
    if (_improv) {
        _improv->handleSerial();
    }
#endif
    wl_status_t status = WiFi.status();

    switch (_state) {
        case WIFI_STATE_DISCONNECTED:
            if (millis() - _lastReconnectAttempt > _reconnectInterval) {
                _lastReconnectAttempt = millis();
                Serial.println("[WiFi] Reconnecting...");
                configureStaticIP();
                WiFi.begin(_ssid.c_str(), _password.c_str());
                _state = WIFI_STATE_CONNECTING;
                _connectionStartTime = millis();
            }
            break;

        case WIFI_STATE_CONNECTING:
            if (status == WL_CONNECTED) {
                _state = WIFI_STATE_CONNECTED;
                Serial.print("[WiFi] Connected! IP address: ");
                Serial.println(WiFi.localIP());
#ifndef NATIVE_TEST
                
#endif
            } else if (status == WL_CONNECT_FAILED || status == WL_NO_SSID_AVAIL || (millis() - _connectionStartTime > _connectionTimeout)) {
                Serial.println("[WiFi] Connection failed or timed out. Transitioning to AP Mode...");
                startAPMode();
            }
            break;

        case WIFI_STATE_CONNECTED:
            if (status != WL_CONNECTED) {
                _state = WIFI_STATE_DISCONNECTED;
                _lastReconnectAttempt = millis();
                Serial.println("[WiFi] Connection lost.");
#ifndef NATIVE_TEST
                
#endif
            } else {
#ifndef NATIVE_TEST
                if (_webServer) {
                    _webServer->handleClient();
                }
#endif
            }
            break;

        case WIFI_STATE_AP_MODE:
            if (WiFi.status() == WL_CONNECTED) {
                Serial.println("[WiFi] Wi-Fi connected in background. Stopping AP Mode...");
#ifndef NATIVE_TEST
                if (_dnsServer) {
                    _dnsServer->stop();
                    delete _dnsServer;
                    _dnsServer = nullptr;
                }
                if (_webServer) {
                    _webServer->stop();
                    delete _webServer;
                    _webServer = nullptr;
                }
                WiFi.softAPdisconnect(true);
                WiFi.mode(WIFI_STA);
                
#endif
                _state = WIFI_STATE_CONNECTED;
                Serial.print("[WiFi] Connected! IP address: ");
                Serial.println(WiFi.localIP());
            } else {
#ifndef NATIVE_TEST
                if (_dnsServer) _dnsServer->processNextRequest();
                if (_webServer) _webServer->handleClient();
#endif
            }
            break;
    }
}

WifiState WifiManager::getState() const {
    return _state;
}

String WifiManager::getIPAddress() const {
    if (_state == WIFI_STATE_CONNECTED) {
        return WiFi.localIP().toString();
    } else if (_state == WIFI_STATE_AP_MODE) {
        return "192.168.4.1";
    }
    return "0.0.0.0";
}

int8_t WifiManager::getRSSI() const {
    if (_state == WIFI_STATE_CONNECTED) {
        return WiFi.RSSI();
    }
    return -100;
}

void WifiManager::setCredentials(const String& ssid, const String& password) {
    _ssid = ssid;
    _password = password;
}

String WifiManager::getAPSSID() {
    String mac = WiFi.macAddress();
    String cleanMac = "";
    for (size_t i = 0; i < mac.length(); i++) {
        if (mac[i] != ':') {
            cleanMac += mac[i];
        }
    }
    String suffix = "";
    if (cleanMac.length() >= 4) {
        suffix = String(cleanMac.c_str() + cleanMac.length() - 4);
    } else {
        suffix = "ESP32";
    }
    for (size_t i = 0; i < suffix.length(); i++) {
        suffix[i] = toupper(suffix[i]);
    }
    return "cyd-flight-radar-" + suffix;
}

void WifiManager::startAPMode() {
    _state = WIFI_STATE_AP_MODE;
    String apSSID = getAPSSID();
    Serial.printf("[WiFi] Entering AP Mode. SSID: %s\n", apSSID.c_str());

#ifndef NATIVE_TEST
    WiFi.persistent(false);
    WiFi.setAutoReconnect(false);
    WiFi.disconnect();
    delay(200);

    WiFi.mode(WIFI_AP_STA);
    WiFi.setTxPower(WIFI_POWER_11dBm);
    delay(100);

    IPAddress apIP(192, 168, 4, 1);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    delay(100);
    
    const char* apPass = nullptr;
#ifdef AP_PASSWORD
    if (strlen(AP_PASSWORD) >= 8) {
        apPass = AP_PASSWORD;
    }
#endif
    WiFi.softAP(apSSID.c_str(), apPass);
    delay(200);

    // Wait for the captive portal to trigger the scan
    _cachedNetworksHTML = "<div class='net-item' style='color: #a6adc8;'>Scanning in progress... Please refresh.</div>";

    _dnsServer = new DNSServer();
    _dnsServer->setErrorReplyCode(DNSReplyCode::NoError);
    _dnsServer->start(53, "*", apIP);

    _webServer = new WebServer(80);
    _webServer->on("/", [this]() { handleRoot(); });
    _webServer->on("/save", [this]() { handleSave(); });
    _webServer->on("/scan", [this]() {
        WiFi.scanNetworks(true, false, false, 150);
        String html = "<!DOCTYPE html><html><head>";
        html += "<meta http-equiv='refresh' content='3;url=/'>";
        html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
        html += "<title>Scanning...</title>";
        html += "<style>";
        html += "body { font-family: 'Inter', system-ui, sans-serif; background: #1e1e2e; color: #cdd6f4; margin: 0; padding: 20px; display: flex; justify-content: center; align-items: center; min-height: 100vh; }";
        html += ".card { background: #181825; border-radius: 12px; padding: 30px; width: 100%; max-width: 400px; box-shadow: 0 8px 30px rgba(0,0,0,0.3); border: 1px solid #313244; text-align: center; }";
        html += "h2 { color: #f5c2e7; margin-top: 0; }";
        html += "p { color: #a6adc8; }";
        html += "</style></head><body>";
        html += "<div class='card'><h2>Scanning for Wi-Fi...</h2><p>Please wait while we refresh the network list.</p></div>";
        html += "</body></html>";
        _webServer->send(200, "text/html", html);
    });
    _webServer->onNotFound([this]() { handleNotFound(); });
    
    _webServer->begin();

    Serial.println("[WiFi] AP Mode Web Server and DNS Server started.");
#endif
}

void WifiManager::handleRoot() {
#ifndef NATIVE_TEST
    int16_t scanStatus = WiFi.scanComplete();
    if (scanStatus >= 0) {
        _cachedNetworksHTML = "";
        for (int i = 0; i < scanStatus; ++i) {
            String ssidName = WiFi.SSID(i);
            int32_t rssi = WiFi.RSSI(i);
            _cachedNetworksHTML += "<div class='net-item' onclick='selectSSID(\"" + ssidName + "\")'>";
            _cachedNetworksHTML += "<span>" + ssidName + "</span>";
            _cachedNetworksHTML += "<span style='color: #a6adc8; font-size: 12px;'>" + String(rssi) + " dBm</span>";
            _cachedNetworksHTML += "</div>";
        }
        WiFi.scanDelete();
    } else if (scanStatus == WIFI_SCAN_FAILED) {
        WiFi.scanNetworks(true, false, false, 150);
        if (_cachedNetworksHTML.length() == 0 || _cachedNetworksHTML.indexOf("Scanning in progress") != -1) {
            _cachedNetworksHTML = "<div class='net-item' style='color: #a6adc8;'>Scanning in progress... Please refresh.</div>";
        }
    }

    String html = "<!DOCTYPE html><html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
    if (scanStatus == WIFI_SCAN_RUNNING || scanStatus == WIFI_SCAN_FAILED) {
        html += "<meta http-equiv='refresh' content='3'>";
    }
    html += "<title>CYD Weather Station Setup</title>";
    html += "<style>";
    html += "body { font-family: 'Inter', system-ui, sans-serif; background: #1e1e2e; color: #cdd6f4; margin: 0; padding: 20px; display: flex; justify-content: center; align-items: center; min-height: 100vh; box-sizing: border-box; }";
    html += ".card { background: #181825; border-radius: 12px; padding: 30px; width: 100%; max-width: 400px; box-shadow: 0 8px 30px rgba(0,0,0,0.3); border: 1px solid #313244; }";
    html += "h2 { color: #f5c2e7; margin-top: 0; margin-bottom: 20px; font-weight: 600; text-align: center; }";
    html += "label { display: block; margin-bottom: 8px; color: #a6adc8; font-size: 14px; }";
    html += "select, input[type='text'], input[type='password'] { width: 100%; padding: 12px; margin-bottom: 20px; border-radius: 6px; border: 1px solid #45475a; background: #313244; color: #cdd6f4; font-size: 16px; box-sizing: border-box; }";
    html += "select:focus, input:focus { outline: none; border-color: #f5c2e7; }";
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
    html += "<h2>Wi-Fi Configuration</h2>";
    html += "<form method='POST' action='/save'>";
    
    html += "<div style='display: flex; justify-content: space-between; align-items: center;'>";
    html += "<label style='margin-bottom: 0;'>Select Network</label>";
    html += "<a href='/scan' style='color: #cba6f7; font-size: 12px; text-decoration: none;'>🔄 Refresh List</a>";
    html += "</div>";
    html += "<div style='height: 8px;'></div>";
    
    html += "<div class='net-list'>";
    html += _cachedNetworksHTML;
    html += "</div>";
    
    html += "<input type='text' id='ssid' name='ssid' placeholder='SSID name' required>";
    
    html += "<input type='password' id='pass' name='pass' placeholder='Password'>";
    
    html += "<label for='lat'>Latitude</label>";
    html += "<input type='text' id='lat' name='lat' placeholder='e.g. 34.1031' value='" + String(services::location::lat(), 4) + "'>";
    
    html += "<label for='lon'>Longitude</label>";
    html += "<input type='text' id='lon' name='lon' placeholder='e.g. -118.416' value='" + String(services::location::lon(), 4) + "'>";
    
    html += "<p style='color: #a6adc8; font-size: 12px; margin-top: -10px; margin-bottom: 20px; text-align: center;'><em>Leave location fields blank to use default.</em></p>";
    
    html += "<button type='submit'>Save & Connect</button>";
    html += "</form>";
    html += "</div>";
    html += "</body></html>";

    _webServer->send(200, "text/html", html);
#endif
}

void WifiManager::handleSave() {
#ifndef NATIVE_TEST
    String ssid = _webServer->arg("ssid");
    String pass = _webServer->arg("pass");
    String zip = _webServer->arg("zip");
    String lat = _webServer->arg("lat");
    String lon = _webServer->arg("lon");
    String tz = _webServer->arg("tz");

    Serial.printf("[WiFi] Saved new credentials via captive portal: %s\n", ssid.c_str());

    Preferences preferences;
    preferences.begin("wifi", false);
    preferences.putString("ssid", ssid);
    preferences.putString("pass", pass);
    preferences.end();
    
    if (lat.length() > 0 && lon.length() > 0) {
        services::location::saveFromStrings(lat.c_str(), lon.c_str());
    }

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
    html += "<p>Connecting to <strong>" + ssid + "</strong>...</p>";
    html += "<p>The device will now reboot to apply the new settings. You can close this page.</p>";
    html += "</div>";
    html += "</body></html>";

    _webServer->send(200, "text/html", html);
    delay(2000);

    
    
    
    ESP.restart();
#endif
}

void WifiManager::handleNotFound() {
#ifndef NATIVE_TEST
    _webServer->sendHeader("Location", "http://192.168.4.1/", true);
    _webServer->send(302, "text/plain", "");
#endif
}

/**
 * @brief Starts or stops the screenshot server based on the given flag.
 */
