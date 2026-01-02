/*
 * Animatronic Eyes
 * Copyright (c) 2025 Zappo-II
 * Licensed under CC BY-NC-SA 4.0
 * https://github.com/Zappo-II/animatronic-eyes
 */

#include "wifi_manager.h"
#include "led_status.h"
#include "config.h"
#include "web_server.h"
#include <WiFi.h>
#include <ESPmDNS.h>

WifiManager wifiManager;

void WifiManager::begin() {
    // Load timing settings
    _timing = storage.getWifiTiming();

    // Start in AP+STA mode
    WiFi.mode(WIFI_AP_STA);

    // Check if any network is configured
    if (storage.hasAnyWifiCredentials()) {
        // Try primary network first
        _currentNetworkIndex = 0;
        WifiNetwork net = storage.getWifiNetwork(0);
        if (net.configured) {
            WEB_LOG("WiFi", "Stored credentials found, connecting to: %s", net.ssid);
            connectToNetwork(0);
        } else {
            // Try secondary if primary not configured
            net = storage.getWifiNetwork(1);
            if (net.configured) {
                _currentNetworkIndex = 1;
                WEB_LOG("WiFi", "Connecting to secondary: %s", net.ssid);
                connectToNetwork(1);
            } else {
                WEB_LOG("WiFi", "No valid stored credentials, starting AP mode");
                startAP();
                _apFallback = false;
            }
        }
    } else {
        WEB_LOG("WiFi", "No stored credentials, starting AP mode");
        startAP();
        _apFallback = false;
    }
}

void WifiManager::loop() {
    switch (_state) {
        case WIFI_STATE_CONNECTING:
            handleConnecting();
            break;

        case WIFI_STATE_CONNECTED:
            checkConnection();
            break;

        case WIFI_STATE_DISCONNECTED:
        case WIFI_STATE_RECONNECTING:
            handleDisconnected();
            break;

        case WIFI_STATE_AP_ONLY:
        case WIFI_STATE_AP_SCANNING:
            handleAPScanning();
            break;

        default:
            break;
    }
}

void WifiManager::handleConnecting() {
    if (WiFi.status() == WL_CONNECTED) {
        _state = WIFI_STATE_CONNECTED;
        _reconnectAttempts = 0;
        _disconnectedSince = 0;

        if (_timing.keepAP) {
            _mode = APP_WIFI_AP_STA;
        } else {
            stopAP();
            _mode = APP_WIFI_STA;
        }

        WEB_LOG("WiFi", "Connected! IP: %s", WiFi.localIP().toString().c_str());

        // Start mDNS
        startMdns();

        updateLedPattern();
    } else if (millis() - _connectStartTime > WIFI_CONNECT_TIMEOUT_MS) {
        WEB_LOG("WiFi", "Connection timeout");

        // Try next network or fall back to AP
        tryNextNetwork();
    }
}

void WifiManager::checkConnection() {
    if (WiFi.status() != WL_CONNECTED) {
        // Connection lost - start grace period
        if (_disconnectedSince == 0) {
            _disconnectedSince = millis();
            WEB_LOG("WiFi", "Connection lost, starting grace period");
        }
        _state = WIFI_STATE_DISCONNECTED;
        updateLedPattern();
    }
}

void WifiManager::handleDisconnected() {
    unsigned long now = millis();
    unsigned long disconnectedFor = now - _disconnectedSince;

    // Check if still within grace period
    if (disconnectedFor < _timing.graceMs) {
        // Still in grace period - check if reconnected
        if (WiFi.status() == WL_CONNECTED) {
            WEB_LOG("WiFi", "Reconnected during grace period");
            _state = WIFI_STATE_CONNECTED;
            _disconnectedSince = 0;
            _reconnectAttempts = 0;
            updateLedPattern();
        }
        return;
    }

    // Grace period expired - need to reconnect
    _state = WIFI_STATE_RECONNECTING;
    updateLedPattern();

    // Check if we should attempt reconnection
    if (now - _lastReconnectAttempt >= _timing.retryDelayMs || _lastReconnectAttempt == 0) {
        if (_reconnectAttempts < _timing.retries) {
            _reconnectAttempts++;
            _lastReconnectAttempt = now;

            WEB_LOG("WiFi", "Reconnect attempt %d/%d to network %d",
                         _reconnectAttempts, _timing.retries, _currentNetworkIndex);

            // Try to reconnect to current network
            WifiNetwork net = storage.getWifiNetwork(_currentNetworkIndex);
            if (net.configured) {
                WiFi.disconnect(true);
                delay(100);
                // Must reset WiFi mode after disconnect - see connectToNetwork() comment
                WiFi.mode(WIFI_AP_STA);
                delay(50);
                WiFi.begin(net.ssid, net.password);
                _connectStartTime = now;
                _state = WIFI_STATE_CONNECTING;
            } else {
                tryNextNetwork();
            }
        } else {
            // All retries exhausted for this network
            WEB_LOG("WiFi", "All retries exhausted for network %d", _currentNetworkIndex);
            tryNextNetwork();
        }
    }
}

void WifiManager::tryNextNetwork() {
    // Reset retry counter
    _reconnectAttempts = 0;

    // Try next network
    uint8_t nextIndex = (_currentNetworkIndex + 1) % WIFI_MAX_NETWORKS;

    // Check if we've tried all networks
    if (nextIndex == 0 && _currentNetworkIndex != 0) {
        // We've cycled through all networks
        WEB_LOG("WiFi", "All networks exhausted, falling back to AP mode");
        startAP();
        _apFallback = true;
        return;
    }

    // Check if next network is configured
    WifiNetwork net = storage.getWifiNetwork(nextIndex);
    if (net.configured) {
        _currentNetworkIndex = nextIndex;
        WEB_LOG("WiFi", "Trying network %d: %s", nextIndex, net.ssid);
        connectToNetwork(nextIndex);
    } else if (nextIndex == 0) {
        // Secondary wasn't configured, we're back at primary which must have failed
        WEB_LOG("WiFi", "No more networks to try, falling back to AP mode");
        startAP();
        _apFallback = true;
    } else {
        // Secondary not configured, already tried primary
        WEB_LOG("WiFi", "No secondary network configured, falling back to AP mode");
        startAP();
        _apFallback = true;
    }
}

void WifiManager::handleAPScanning() {
    // Only scan if we fell back to AP and have stored credentials
    if (!_apFallback || !storage.hasAnyWifiCredentials()) {
        return;
    }

    unsigned long now = millis();
    if (now - _lastAPScan >= _timing.apScanMs) {
        _lastAPScan = now;

        WEB_LOG("WiFi", "AP mode: scanning for known networks...");

        // Try primary first
        WifiNetwork net = storage.getWifiNetwork(0);
        if (net.configured) {
            _currentNetworkIndex = 0;
            _reconnectAttempts = 0;
            connectToNetwork(0);
            return;
        }

        // Try secondary
        net = storage.getWifiNetwork(1);
        if (net.configured) {
            _currentNetworkIndex = 1;
            _reconnectAttempts = 0;
            connectToNetwork(1);
        }
    }
}

void WifiManager::connectToNetwork(uint8_t index) {
    WifiNetwork net = storage.getWifiNetwork(index);
    if (!net.configured) {
        WEB_LOG("WiFi", "Network %d not configured", index);
        return;
    }

    connectToNetwork(net.ssid, net.password);
    _currentNetworkIndex = index;
}

void WifiManager::connectToNetwork(const char* ssid, const char* password) {
    WiFi.disconnect(true);
    delay(100);

    // IMPORTANT: WiFi.mode() MUST be called before WiFi.begin() after disconnect.
    // WiFi.disconnect(true) can reset the WiFi mode internally. If we don't
    // explicitly set it again, WiFi.begin() may silently fail to connect.
    // This caused reconnection failures after factory reset. (Fixed in v0.2.16)
    WiFi.mode(WIFI_AP_STA);
    delay(50);

    WEB_LOG("WiFi", "Connecting to: %s", ssid);
    WiFi.begin(ssid, password);

    _state = WIFI_STATE_CONNECTING;
    _connectStartTime = millis();
    _lastReconnectAttempt = millis();
    _disconnectedSince = 0;

    updateLedPattern();
}

void WifiManager::startAP() {
    String apName = generateAPName();
    ApConfig apConfig = storage.getApConfig();

    // Make sure AP is started
    if (!WiFi.softAP(apName.c_str(), apConfig.password, AP_CHANNEL)) {
        WEB_LOG("WiFi", "ERROR: Failed to start AP");
        return;
    }

    _mode = APP_WIFI_AP;
    _state = _apFallback ? WIFI_STATE_AP_SCANNING : WIFI_STATE_AP_ONLY;
    _lastAPScan = millis();

    WEB_LOG("WiFi", "AP started: %s (IP: %s)", apName.c_str(), WiFi.softAPIP().toString().c_str());

    updateLedPattern();
}

void WifiManager::stopAP() {
    WiFi.softAPdisconnect(true);
    WEB_LOG("WiFi", "AP stopped");
}

void WifiManager::disconnect() {
    stopMdns();
    WiFi.disconnect(true);
    storage.clearAllWifiNetworks();
    _mode = APP_WIFI_NONE;
    _state = WIFI_STATE_IDLE;
    _reconnectAttempts = 0;
    _disconnectedSince = 0;
    _apFallback = false;

    startAP();
}

void WifiManager::resetConnection() {
    // Drop current connection but keep credentials
    WEB_LOG("WiFi", "Reset connection requested");
    stopMdns();
    WiFi.disconnect(true);
    delay(100);

    // Reset state
    _reconnectAttempts = 0;
    _disconnectedSince = 0;
    _currentNetworkIndex = 0;  // Start from primary

    // Start fresh connection attempt
    WiFi.mode(WIFI_AP_STA);
    delay(50);

    if (storage.hasAnyWifiCredentials()) {
        WifiNetwork net = storage.getWifiNetwork(0);
        if (net.configured) {
            WEB_LOG("WiFi", "Reconnecting to primary: %s", net.ssid);
            connectToNetwork(0);
        } else {
            net = storage.getWifiNetwork(1);
            if (net.configured) {
                _currentNetworkIndex = 1;
                WEB_LOG("WiFi", "Reconnecting to secondary: %s", net.ssid);
                connectToNetwork(1);
            } else {
                WEB_LOG("WiFi", "No networks configured");
                startAP();
                _apFallback = false;
            }
        }
    } else {
        WEB_LOG("WiFi", "No stored credentials");
        startAP();
        _apFallback = false;
    }
}

String WifiManager::scanNetworks() {
    WEB_LOG("WiFi", "Scanning for networks...");

    int numNetworks = WiFi.scanNetworks(false, false, false, 300);  // Active scan, 300ms per channel

    String json = "[";

    if (numNetworks > 0) {
        WEB_LOG("WiFi", "Found %d networks", numNetworks);

        for (int i = 0; i < numNetworks; i++) {
            if (i > 0) json += ",";
            json += "{";
            json += "\"ssid\":\"" + WiFi.SSID(i) + "\",";
            json += "\"rssi\":" + String(WiFi.RSSI(i)) + ",";
            json += "\"secure\":" + String(WiFi.encryptionType(i) != WIFI_AUTH_OPEN ? "true" : "false");
            json += "}";
        }
    } else {
        WEB_LOG("WiFi", "No networks found");
    }

    json += "]";

    WiFi.scanDelete();  // Free memory

    return json;
}

// mDNS functions
void WifiManager::startMdns() {
    MdnsConfig config = storage.getMdnsConfig();

    if (!config.enabled) {
        WEB_LOG("mDNS", "Disabled in settings");
        return;
    }

    if (_mdnsActive) {
        MDNS.end();
    }

    // Always append device ID to hostname (like AP naming)
    uint64_t chipId = ESP.getEfuseMac();
    char fullHostname[80];
    snprintf(fullHostname, sizeof(fullHostname), "%s-%06X", config.hostname, (uint32_t)(chipId & 0xFFFFFF));

    if (MDNS.begin(fullHostname)) {
        MDNS.addService("http", "tcp", HTTP_PORT);
        _mdnsActive = true;
        WEB_LOG("mDNS", "Advertising as %s.local", fullHostname);
    } else {
        WEB_LOG("mDNS", "ERROR: Failed to start mDNS");
        _mdnsActive = false;
    }
}

void WifiManager::stopMdns() {
    if (_mdnsActive) {
        MDNS.end();
        _mdnsActive = false;
        WEB_LOG("mDNS", "Stopped");
    }
}

bool WifiManager::isMdnsActive() {
    return _mdnsActive;
}

// State accessors
AppWifiMode WifiManager::getMode() {
    return _mode;
}

WifiState WifiManager::getState() {
    return _state;
}

String WifiManager::getSSID() {
    if (_state == WIFI_STATE_CONNECTED || _state == WIFI_STATE_CONNECTING) {
        return WiFi.SSID();
    }
    return "";
}

String WifiManager::getIP() {
    if (_state == WIFI_STATE_CONNECTED) {
        return WiFi.localIP().toString();
    }
    return "0.0.0.0";
}

String WifiManager::getAPIP() {
    return WiFi.softAPIP().toString();
}

String WifiManager::getAPName() {
    return generateAPName();
}

bool WifiManager::isConnected() {
    return _state == WIFI_STATE_CONNECTED && WiFi.status() == WL_CONNECTED;
}

bool WifiManager::isAPActive() {
    return _mode == APP_WIFI_AP || _mode == APP_WIFI_AP_STA;
}

bool WifiManager::isReconnecting() {
    return _state == WIFI_STATE_RECONNECTING || _state == WIFI_STATE_CONNECTING;
}

uint8_t WifiManager::getReconnectAttempt() {
    return _reconnectAttempts;
}

uint8_t WifiManager::getCurrentNetworkIndex() {
    return _currentNetworkIndex;
}

String WifiManager::generateAPName() {
    ApConfig apConfig = storage.getApConfig();
    uint64_t chipId = ESP.getEfuseMac();
    char apName[48];
    snprintf(apName, sizeof(apName), "%s-%06X", apConfig.ssidPrefix, (uint32_t)(chipId & 0xFFFFFF));
    return String(apName);
}

void WifiManager::updateLedPattern() {
    switch (_state) {
        case WIFI_STATE_CONNECTED:
            ledStatus.solid();
            break;

        case WIFI_STATE_CONNECTING:
        case WIFI_STATE_RECONNECTING:
            ledStatus.fastBlink();
            break;

        case WIFI_STATE_AP_ONLY:
            ledStatus.slowBlink();
            break;

        case WIFI_STATE_AP_SCANNING:
            ledStatus.doubleBlink();
            break;

        case WIFI_STATE_DISCONNECTED:
            ledStatus.fastBlink();
            break;

        default:
            ledStatus.off();
            break;
    }
}
