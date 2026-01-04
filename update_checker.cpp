/*
 * Animatronic Eyes
 * Copyright (c) 2025 Zappo-II
 * Licensed under CC BY-NC-SA 4.0
 * https://github.com/Zappo-II/animatronic-eyes
 */

#include "update_checker.h"
#include "wifi_manager.h"
#include "web_server.h"
#include <WiFiClientSecure.h>

UpdateChecker updateChecker;

void UpdateChecker::begin() {
    UpdateCheckConfig config = storage.getUpdateCheckConfig();
    _enabled = config.enabled;
    _interval = config.interval;

    UpdateCheckCache cache = storage.getUpdateCheckCache();
    _lastCheckTime = cache.lastCheckTime;
    _updateAvailable = cache.updateAvailable;
    strncpy(_availableVersion, cache.availableVersion, sizeof(_availableVersion) - 1);
    _availableVersion[sizeof(_availableVersion) - 1] = '\0';

    // Clear cache if firmware updated
    if (_updateAvailable && strlen(_availableVersion) > 0) {
        if (!isNewerVersion(_availableVersion, FIRMWARE_VERSION)) {
            _updateAvailable = false;
            _availableVersion[0] = '\0';
            storage.clearUpdateCheckCache();
        }
    }

    if (_enabled) {
        _nextCheckTime = millis() + UPDATE_CHECK_BOOT_DELAY_MS + random(UPDATE_CHECK_JITTER_MS);
    }
}

void UpdateChecker::loop() {
    if (!_enabled && !_checkInProgress) return;
    if (shouldCheck()) performCheck();
}

bool UpdateChecker::shouldCheck() {
    if (_checkInProgress) return false;
    if (!wifiManager.isConnected()) return false;
    return millis() >= _nextCheckTime;
}

void UpdateChecker::checkNow() {
    if (_checkInProgress || !wifiManager.isConnected()) return;
    performCheck();
}

// Minimal HTTPS GET without HTTPClient (saves ~15KB)
void UpdateChecker::performCheck() {
    _checkInProgress = true;

    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(10);

    if (!client.connect("raw.githubusercontent.com", 443)) {
        _checkInProgress = false;
        scheduleNextCheck();
        return;
    }

    // Send minimal HTTP request
    client.println("GET /Zappo-II/animatronic-eyes/main/data/version.json HTTP/1.0");
    client.println("Host: raw.githubusercontent.com");
    client.println("Connection: close");
    client.println();

    // Skip headers, find body
    while (client.connected()) {
        String line = client.readStringUntil('\n');
        if (line == "\r") break;
    }

    // Read body (small JSON)
    String body = client.readString();
    client.stop();

    // Simple parse: find "version":"X.Y.Z"
    int vIdx = body.indexOf("\"version\"");
    if (vIdx > 0) {
        int q1 = body.indexOf('"', vIdx + 9);
        int q2 = body.indexOf('"', q1 + 1);
        if (q1 > 0 && q2 > q1) {
            String ver = body.substring(q1 + 1, q2);
            if (ver.length() > 0 && ver.length() < 16) {
                if (isNewerVersion(ver.c_str(), FIRMWARE_VERSION)) {
                    _updateAvailable = true;
                    strncpy(_availableVersion, ver.c_str(), sizeof(_availableVersion) - 1);
                    _availableVersion[sizeof(_availableVersion) - 1] = '\0';
                } else {
                    _updateAvailable = false;
                    _availableVersion[0] = '\0';
                }

                _lastCheckTime = millis();
                UpdateCheckCache cache;
                cache.lastCheckTime = _lastCheckTime;
                cache.updateAvailable = _updateAvailable;
                strncpy(cache.availableVersion, _availableVersion, sizeof(cache.availableVersion) - 1);
                cache.availableVersion[sizeof(cache.availableVersion) - 1] = '\0';
                storage.setUpdateCheckCache(cache);
                _bootCheckDone = true;
            }
        }
    }

    _checkInProgress = false;
    scheduleNextCheck();
}

void UpdateChecker::scheduleNextCheck() {
    unsigned long intervalMs = getIntervalMs();

    if (intervalMs == 0) {
        // Boot-only mode - set far future
        _nextCheckTime = 0xFFFFFFFF;
        return;
    }

    // Add random jitter to prevent thundering herd
    unsigned long jitter = random(UPDATE_CHECK_JITTER_MS);
    _nextCheckTime = millis() + intervalMs + jitter;

    WEB_LOG("UpdateChecker", "Next check in %lu ms", intervalMs + jitter);
}

unsigned long UpdateChecker::getIntervalMs() const {
    switch (_interval) {
        case 0: return UPDATE_INTERVAL_BOOT_ONLY;
        case 1: return UPDATE_INTERVAL_DAILY;
        case 2: return UPDATE_INTERVAL_WEEKLY;
        default: return UPDATE_INTERVAL_DAILY;
    }
}

bool UpdateChecker::isNewerVersion(const char* remote, const char* local) {
    int rMajor = 0, rMinor = 0, rPatch = 0;
    int lMajor = 0, lMinor = 0, lPatch = 0;

    sscanf(remote, "%d.%d.%d", &rMajor, &rMinor, &rPatch);
    sscanf(local, "%d.%d.%d", &lMajor, &lMinor, &lPatch);

    if (rMajor != lMajor) return rMajor > lMajor;
    if (rMinor != lMinor) return rMinor > lMinor;
    return rPatch > lPatch;
}

// State getters
bool UpdateChecker::isUpdateAvailable() const {
    return _updateAvailable;
}

const char* UpdateChecker::getAvailableVersion() const {
    return _availableVersion;
}

unsigned long UpdateChecker::getLastCheckTime() const {
    return _lastCheckTime;
}

bool UpdateChecker::isCheckInProgress() const {
    return _checkInProgress;
}

// Config setters
void UpdateChecker::setEnabled(bool enabled) {
    _enabled = enabled;

    // Persist to storage
    UpdateCheckConfig config;
    config.enabled = _enabled;
    config.interval = _interval;
    storage.setUpdateCheckConfig(config);

    if (enabled && !_bootCheckDone) {
        // Schedule check soon
        _nextCheckTime = millis() + UPDATE_CHECK_BOOT_DELAY_MS;
    }
}

void UpdateChecker::setInterval(uint8_t interval) {
    _interval = interval;

    // Persist to storage
    UpdateCheckConfig config;
    config.enabled = _enabled;
    config.interval = _interval;
    storage.setUpdateCheckConfig(config);

    // Reschedule next check
    scheduleNextCheck();
}

bool UpdateChecker::isEnabled() const {
    return _enabled;
}

uint8_t UpdateChecker::getInterval() const {
    return _interval;
}
