/*
 * Animatronic Eyes
 * Copyright (c) 2025 Zappo-II
 * Licensed under CC BY-NC-SA 4.0
 * https://github.com/Zappo-II/animatronic-eyes
 */

#include "storage.h"
#include <Preferences.h>

Storage storage;

static Preferences prefs;

static const uint8_t DEFAULT_PINS[NUM_SERVOS] = {
    DEFAULT_PIN_LEFT_EYE_X,
    DEFAULT_PIN_LEFT_EYE_Y,
    DEFAULT_PIN_LEFT_EYELID,
    DEFAULT_PIN_RIGHT_EYE_X,
    DEFAULT_PIN_RIGHT_EYE_Y,
    DEFAULT_PIN_RIGHT_EYELID
};

void Storage::begin() {
    prefs.begin(NVS_NAMESPACE, false);
}

// Helper for WiFi network keys
String Storage::wifiKey(uint8_t index, const char* suffix) {
    char key[16];
    snprintf(key, sizeof(key), "wifi%d_%s", index, suffix);
    return String(key);
}

// Check if any WiFi network is configured
bool Storage::hasAnyWifiCredentials() {
    for (uint8_t i = 0; i < WIFI_MAX_NETWORKS; i++) {
        String ssidKey = wifiKey(i, "ssid");
        if (prefs.isKey(ssidKey.c_str()) && prefs.getString(ssidKey.c_str(), "").length() > 0) {
            return true;
        }
    }
    return false;
}

// Get WiFi network by index
WifiNetwork Storage::getWifiNetwork(uint8_t index) {
    WifiNetwork network;
    memset(&network, 0, sizeof(network));
    network.configured = false;

    if (index >= WIFI_MAX_NETWORKS) {
        return network;
    }

    String ssidKey = wifiKey(index, "ssid");
    String passKey = wifiKey(index, "pass");

    String ssid = prefs.getString(ssidKey.c_str(), "");
    String pass = prefs.getString(passKey.c_str(), "");

    if (ssid.length() > 0) {
        strncpy(network.ssid, ssid.c_str(), sizeof(network.ssid) - 1);
        strncpy(network.password, pass.c_str(), sizeof(network.password) - 1);
        network.ssid[sizeof(network.ssid) - 1] = '\0';
        network.password[sizeof(network.password) - 1] = '\0';
        network.configured = true;
    }

    return network;
}

// Set WiFi network by index
bool Storage::setWifiNetwork(uint8_t index, const char* ssid, const char* password) {
    if (index >= WIFI_MAX_NETWORKS) {
        return false;
    }

    String ssidKey = wifiKey(index, "ssid");
    String passKey = wifiKey(index, "pass");

    size_t ssidWritten = prefs.putString(ssidKey.c_str(), ssid);
    size_t passWritten = prefs.putString(passKey.c_str(), password);

    // IMPORTANT: NVS writes can fail silently, especially after crashes or when
    // flash is worn. Always verify writes by reading back. Without this check,
    // WiFi credentials appeared to save but weren't persisted. (Fixed in v0.2.16)
    if (ssidWritten == 0) {
        return false;
    }

    // Verify by reading back
    String savedSsid = prefs.getString(ssidKey.c_str(), "");
    if (savedSsid != ssid) {
        return false;
    }

    return true;
}

// Clear WiFi network by index
void Storage::clearWifiNetwork(uint8_t index) {
    if (index >= WIFI_MAX_NETWORKS) {
        return;
    }

    prefs.remove(wifiKey(index, "ssid").c_str());
    prefs.remove(wifiKey(index, "pass").c_str());
}

// Clear all WiFi networks
void Storage::clearAllWifiNetworks() {
    for (uint8_t i = 0; i < WIFI_MAX_NETWORKS; i++) {
        clearWifiNetwork(i);
    }
}

// Get WiFi timing settings
WifiTiming Storage::getWifiTiming() {
    WifiTiming timing;
    timing.graceMs = prefs.getUShort("wifi_grace", DEFAULT_WIFI_GRACE_MS);
    timing.retries = prefs.getUChar("wifi_retries", DEFAULT_WIFI_RETRIES);
    timing.retryDelayMs = prefs.getUShort("wifi_retry_d", DEFAULT_WIFI_RETRY_DELAY_MS);
    timing.apScanMs = prefs.getULong("wifi_ap_scan", DEFAULT_WIFI_AP_SCAN_MS);
    timing.keepAP = prefs.getBool("wifi_keep_ap", DEFAULT_WIFI_KEEP_AP);
    return timing;
}

// Set WiFi timing settings
void Storage::setWifiTiming(const WifiTiming& timing) {
    prefs.putUShort("wifi_grace", timing.graceMs);
    prefs.putUChar("wifi_retries", timing.retries);
    prefs.putUShort("wifi_retry_d", timing.retryDelayMs);
    prefs.putULong("wifi_ap_scan", timing.apScanMs);
    prefs.putBool("wifi_keep_ap", timing.keepAP);
}

// Legacy compatibility - uses network 0
bool Storage::hasWifiCredentials() {
    WifiNetwork net = getWifiNetwork(0);
    return net.configured;
}

WifiNetwork Storage::getWifiConfig() {
    return getWifiNetwork(0);
}

bool Storage::setWifiConfig(const char* ssid, const char* password) {
    return setWifiNetwork(0, ssid, password);
}

void Storage::clearWifiConfig() {
    clearWifiNetwork(0);
}

// Servo key helper
String Storage::servoKey(uint8_t index, const char* suffix) {
    char key[16];
    snprintf(key, sizeof(key), "sv%d_%s", index, suffix);
    return String(key);
}

ServoConfig Storage::getServoConfig(uint8_t index) {
    ServoConfig config;
    if (index >= NUM_SERVOS) {
        config.pin = DEFAULT_PINS[0];
        config.min = DEFAULT_SERVO_MIN;
        config.center = DEFAULT_SERVO_CENTER;
        config.max = DEFAULT_SERVO_MAX;
        config.invert = false;
        return config;
    }

    config.pin = prefs.getUChar(servoKey(index, "pin").c_str(), DEFAULT_PINS[index]);
    config.min = prefs.getUChar(servoKey(index, "min").c_str(), DEFAULT_SERVO_MIN);
    config.center = prefs.getUChar(servoKey(index, "ctr").c_str(), DEFAULT_SERVO_CENTER);
    config.max = prefs.getUChar(servoKey(index, "max").c_str(), DEFAULT_SERVO_MAX);
    config.invert = prefs.getBool(servoKey(index, "inv").c_str(), false);

    return config;
}

void Storage::setServoConfig(uint8_t index, const ServoConfig& config) {
    if (index >= NUM_SERVOS) return;

    prefs.putUChar(servoKey(index, "pin").c_str(), config.pin);
    prefs.putUChar(servoKey(index, "min").c_str(), config.min);
    prefs.putUChar(servoKey(index, "ctr").c_str(), config.center);
    prefs.putUChar(servoKey(index, "max").c_str(), config.max);
    prefs.putBool(servoKey(index, "inv").c_str(), config.invert);
}

void Storage::setServoPin(uint8_t index, uint8_t pin) {
    if (index >= NUM_SERVOS) return;
    prefs.putUChar(servoKey(index, "pin").c_str(), pin);
}

void Storage::setServoCalibration(uint8_t index, uint8_t min, uint8_t center, uint8_t max) {
    if (index >= NUM_SERVOS) return;
    prefs.putUChar(servoKey(index, "min").c_str(), min);
    prefs.putUChar(servoKey(index, "ctr").c_str(), center);
    prefs.putUChar(servoKey(index, "max").c_str(), max);
}

void Storage::setServoInvert(uint8_t index, bool invert) {
    if (index >= NUM_SERVOS) return;
    prefs.putBool(servoKey(index, "inv").c_str(), invert);
}

// LED config
LedConfig Storage::getLedConfig() {
    LedConfig config;
    config.enabled = prefs.getBool("led_enabled", DEFAULT_LED_ENABLED);
    config.pin = prefs.getUChar("led_pin", DEFAULT_LED_PIN);
    config.brightness = prefs.getUChar("led_bright", DEFAULT_LED_BRIGHTNESS);
    return config;
}

void Storage::setLedConfig(const LedConfig& config) {
    prefs.putBool("led_enabled", config.enabled);
    prefs.putUChar("led_pin", config.pin);
    prefs.putUChar("led_bright", config.brightness);
}

// mDNS config
MdnsConfig Storage::getMdnsConfig() {
    MdnsConfig config;
    config.enabled = prefs.getBool("mdns_enabled", DEFAULT_MDNS_ENABLED);
    String hostname = prefs.getString("mdns_host", DEFAULT_MDNS_HOSTNAME);
    strncpy(config.hostname, hostname.c_str(), sizeof(config.hostname) - 1);
    config.hostname[sizeof(config.hostname) - 1] = '\0';
    return config;
}

void Storage::setMdnsConfig(const MdnsConfig& config) {
    // Check if hostname changed (requires reboot)
    MdnsConfig current = getMdnsConfig();
    if (strcmp(current.hostname, config.hostname) != 0) {
        _rebootRequired = true;
    }

    prefs.putBool("mdns_enabled", config.enabled);
    prefs.putString("mdns_host", config.hostname);
}

// AP config
ApConfig Storage::getApConfig() {
    ApConfig config;
    String prefix = prefs.getString("ap_prefix", DEFAULT_AP_SSID_PREFIX);
    String password = prefs.getString("ap_pass", DEFAULT_AP_PASSWORD);
    strncpy(config.ssidPrefix, prefix.c_str(), sizeof(config.ssidPrefix) - 1);
    config.ssidPrefix[sizeof(config.ssidPrefix) - 1] = '\0';
    strncpy(config.password, password.c_str(), sizeof(config.password) - 1);
    config.password[sizeof(config.password) - 1] = '\0';
    return config;
}

void Storage::setApConfig(const ApConfig& config) {
    prefs.putString("ap_prefix", config.ssidPrefix);
    prefs.putString("ap_pass", config.password);
    _rebootRequired = true;  // AP changes require reboot
}

// Reboot required flag
bool Storage::isRebootRequired() {
    return _rebootRequired;
}

void Storage::setRebootRequired(bool required) {
    _rebootRequired = required;
}

void Storage::clearRebootRequired() {
    _rebootRequired = false;
}

// Device password (legacy - delegates to AP config)
String Storage::getDevicePassword() {
    ApConfig config = getApConfig();
    return String(config.password);
}

void Storage::setDevicePassword(const char* password) {
    ApConfig config = getApConfig();
    strncpy(config.password, password, sizeof(config.password) - 1);
    config.password[sizeof(config.password) - 1] = '\0';
    setApConfig(config);
}

// Mode config
ModeConfig Storage::getModeConfig() {
    ModeConfig config;
    String mode = prefs.getString("mode_def", DEFAULT_MODE);
    strncpy(config.defaultMode, mode.c_str(), sizeof(config.defaultMode) - 1);
    config.defaultMode[sizeof(config.defaultMode) - 1] = '\0';
    config.autoBlink = prefs.getBool("mode_ablink", DEFAULT_AUTO_BLINK);
    config.blinkIntervalMin = prefs.getUShort("mode_blkmin", DEFAULT_BLINK_INTERVAL_MIN);
    config.blinkIntervalMax = prefs.getUShort("mode_blkmax", DEFAULT_BLINK_INTERVAL_MAX);
    config.rememberLastMode = prefs.getBool("mode_remember", false);
    return config;
}

void Storage::setModeConfig(const ModeConfig& config) {
    prefs.putString("mode_def", config.defaultMode);
    prefs.putBool("mode_ablink", config.autoBlink);
    prefs.putUShort("mode_blkmin", config.blinkIntervalMin);
    prefs.putUShort("mode_blkmax", config.blinkIntervalMax);
    prefs.putBool("mode_remember", config.rememberLastMode);
}

// Impulse config
ImpulseConfig Storage::getImpulseConfig() {
    ImpulseConfig config;
    config.autoImpulse = prefs.getBool("imp_auto", DEFAULT_AUTO_IMPULSE);
    config.impulseIntervalMin = prefs.getULong("imp_intmin", DEFAULT_IMPULSE_INTERVAL_MIN);
    config.impulseIntervalMax = prefs.getULong("imp_intmax", DEFAULT_IMPULSE_INTERVAL_MAX);
    String selection = prefs.getString("imp_select", DEFAULT_IMPULSE_SELECTION);
    strncpy(config.impulseSelection, selection.c_str(), sizeof(config.impulseSelection) - 1);
    config.impulseSelection[sizeof(config.impulseSelection) - 1] = '\0';
    return config;
}

void Storage::setImpulseConfig(const ImpulseConfig& config) {
    prefs.putBool("imp_auto", config.autoImpulse);
    prefs.putULong("imp_intmin", config.impulseIntervalMin);
    prefs.putULong("imp_intmax", config.impulseIntervalMax);
    prefs.putString("imp_select", config.impulseSelection);
}

// Admin PIN
bool Storage::hasAdminPin() {
    return prefs.isKey("admin_pin") && prefs.getString("admin_pin", "").length() > 0;
}

String Storage::getAdminPin() {
    return prefs.getString("admin_pin", "");
}

bool Storage::setAdminPin(const char* pin) {
    // Validate: must be 4-6 digits
    size_t len = strlen(pin);
    if (len < 4 || len > 6) {
        return false;
    }
    for (size_t i = 0; i < len; i++) {
        if (!isdigit(pin[i])) {
            return false;
        }
    }
    prefs.putString("admin_pin", pin);
    return true;
}

void Storage::clearAdminPin() {
    prefs.remove("admin_pin");
}

// Factory reset
void Storage::factoryReset() {
    // Close current namespace
    prefs.end();

    // Reopen and clear
    prefs.begin(NVS_NAMESPACE, false);
    prefs.clear();

    // Force remove WiFi keys explicitly
    for (uint8_t i = 0; i < WIFI_MAX_NETWORKS; i++) {
        prefs.remove(wifiKey(i, "ssid").c_str());
        prefs.remove(wifiKey(i, "pass").c_str());
    }

    // Close cleanly before reboot
    prefs.end();
}
