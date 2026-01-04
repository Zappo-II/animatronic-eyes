/*
 * Animatronic Eyes
 * Copyright (c) 2025 Zappo-II
 * Licensed under CC BY-NC-SA 4.0
 * https://github.com/Zappo-II/animatronic-eyes
 */

#ifndef STORAGE_H
#define STORAGE_H

#include <Arduino.h>
#include "config.h"

struct ServoConfig {
    uint8_t pin;
    uint8_t min;
    uint8_t center;
    uint8_t max;
    bool invert;
};

struct WifiNetwork {
    char ssid[33];
    char password[65];
    bool configured;
};

struct WifiTiming {
    uint16_t graceMs;       // 1000-10000 (1-10s)
    uint8_t retries;        // 1-10
    uint16_t retryDelayMs;  // 5000-60000 (5-60s)
    uint32_t apScanMs;      // 60000-1800000 (1-30 min)
    bool keepAP;            // Keep AP active when STA connected
};

struct LedConfig {
    bool enabled;
    uint8_t pin;
    uint8_t brightness;  // 1-255 (PWM duty cycle)
};

struct MdnsConfig {
    bool enabled;
    char hostname[64];
};

struct ApConfig {
    char ssidPrefix[24];  // e.g., "LookIntoMyEyes"
    char password[65];    // WPA2 min 8 chars
};

struct ModeConfig {
    char defaultMode[32];     // "follow" or auto mode name (e.g., "natural")
    bool autoBlink;           // Enable periodic automatic blinking
    uint16_t blinkIntervalMin; // Minimum ms between auto-blinks
    uint16_t blinkIntervalMax; // Maximum ms between auto-blinks
    bool rememberLastMode;    // If true, restore last active mode on boot
};

// Maximum number of impulses that can be selected for auto-impulse
#define MAX_IMPULSE_SELECTION 8
#define IMPULSE_SELECTION_STRLEN 256  // Max length of comma-separated selection string

struct ImpulseConfig {
    bool autoImpulse;              // Enable periodic automatic impulses
    uint32_t impulseIntervalMin;   // Minimum ms between auto-impulses (30000 = 30s)
    uint32_t impulseIntervalMax;   // Maximum ms between auto-impulses (120000 = 2min)
    char impulseSelection[IMPULSE_SELECTION_STRLEN];  // Comma-separated list of selected impulse names
};

struct UpdateCheckConfig {
    bool enabled;           // Master enable/disable for update checking
    uint8_t interval;       // 0=boot only, 1=daily, 2=weekly
};

struct UpdateCheckCache {
    uint32_t lastCheckTime;        // millis() when last check was performed
    char availableVersion[16];     // Version found on GitHub (empty if up-to-date)
    bool updateAvailable;          // True if newer version found
};

class Storage {
public:
    void begin();

    // WiFi Networks (multi-SSID)
    bool hasAnyWifiCredentials();
    WifiNetwork getWifiNetwork(uint8_t index);  // 0 = primary, 1 = secondary
    bool setWifiNetwork(uint8_t index, const char* ssid, const char* password);
    void clearWifiNetwork(uint8_t index);
    void clearAllWifiNetworks();

    // WiFi Timing
    WifiTiming getWifiTiming();
    void setWifiTiming(const WifiTiming& timing);

    // Legacy compatibility (uses network 0)
    bool hasWifiCredentials();
    WifiNetwork getWifiConfig();
    bool setWifiConfig(const char* ssid, const char* password);
    void clearWifiConfig();

    // Servos
    ServoConfig getServoConfig(uint8_t index);
    void setServoConfig(uint8_t index, const ServoConfig& config);
    void setServoPin(uint8_t index, uint8_t pin);
    void setServoCalibration(uint8_t index, uint8_t min, uint8_t center, uint8_t max);
    void setServoInvert(uint8_t index, bool invert);

    // LED Status
    LedConfig getLedConfig();
    void setLedConfig(const LedConfig& config);

    // mDNS
    MdnsConfig getMdnsConfig();
    void setMdnsConfig(const MdnsConfig& config);

    // AP Mode
    ApConfig getApConfig();
    void setApConfig(const ApConfig& config);

    // Mode System
    ModeConfig getModeConfig();
    void setModeConfig(const ModeConfig& config);

    // Impulse System
    ImpulseConfig getImpulseConfig();
    void setImpulseConfig(const ImpulseConfig& config);

    // Update Check
    UpdateCheckConfig getUpdateCheckConfig();
    void setUpdateCheckConfig(const UpdateCheckConfig& config);
    UpdateCheckCache getUpdateCheckCache();
    void setUpdateCheckCache(const UpdateCheckCache& cache);
    void clearUpdateCheckCache();

    // Admin PIN (4-6 digits, stored plain text)
    bool hasAdminPin();
    String getAdminPin();
    bool setAdminPin(const char* pin);  // Returns false if invalid (not 4-6 digits)
    void clearAdminPin();

    // Reboot required flag (in-memory, not persisted)
    bool isRebootRequired();
    void setRebootRequired(bool required);
    void clearRebootRequired();

    // Device (legacy - use getApConfig instead)
    String getDevicePassword();
    void setDevicePassword(const char* password);

    // Factory reset
    void factoryReset();

private:
    bool _rebootRequired = false;
    void loadDefaults();
    String servoKey(uint8_t index, const char* suffix);
    String wifiKey(uint8_t index, const char* suffix);
};

extern Storage storage;

#endif // STORAGE_H
