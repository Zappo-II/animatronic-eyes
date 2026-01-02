/*
 * Animatronic Eyes
 * Copyright (c) 2025 Zappo-II
 * Licensed under CC BY-NC-SA 4.0
 * https://github.com/Zappo-II/animatronic-eyes
 */

#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include "storage.h"

enum AppWifiMode {
    APP_WIFI_NONE,
    APP_WIFI_AP,
    APP_WIFI_STA,
    APP_WIFI_AP_STA  // Both active
};

enum WifiState {
    WIFI_STATE_IDLE,
    WIFI_STATE_CONNECTING,
    WIFI_STATE_CONNECTED,
    WIFI_STATE_DISCONNECTED,
    WIFI_STATE_RECONNECTING,
    WIFI_STATE_AP_ONLY,
    WIFI_STATE_AP_SCANNING
};

class WifiManager {
public:
    void begin();
    void loop();

    // State accessors
    AppWifiMode getMode();
    WifiState getState();
    String getSSID();
    String getIP();
    String getAPIP();
    String getAPName();
    bool isConnected();
    bool isAPActive();
    bool isReconnecting();
    uint8_t getReconnectAttempt();
    uint8_t getCurrentNetworkIndex();

    // Actions
    void connectToNetwork(uint8_t index);
    void connectToNetwork(const char* ssid, const char* password);
    void startAP();
    void stopAP();
    void disconnect();
    void tryNextNetwork();
    void resetConnection();  // Drop connection, trigger reconnect cycle (keeps credentials)
    String scanNetworks();   // Returns JSON array of available networks

    // mDNS
    void startMdns();
    void stopMdns();
    bool isMdnsActive();

private:
    AppWifiMode _mode = APP_WIFI_NONE;
    WifiState _state = WIFI_STATE_IDLE;

    // Connection tracking
    uint8_t _currentNetworkIndex = 0;
    unsigned long _connectStartTime = 0;
    unsigned long _disconnectedSince = 0;
    unsigned long _lastReconnectAttempt = 0;
    uint8_t _reconnectAttempts = 0;

    // AP mode scanning
    unsigned long _lastAPScan = 0;
    bool _apFallback = false;  // True if AP is due to STA failure

    // mDNS
    bool _mdnsActive = false;

    // Timing (loaded from storage)
    WifiTiming _timing;

    // Helpers
    String generateAPName();
    void checkConnection();
    void handleConnecting();
    void handleDisconnected();
    void handleAPScanning();
    void updateLedPattern();
};

extern WifiManager wifiManager;

#endif // WIFI_MANAGER_H
