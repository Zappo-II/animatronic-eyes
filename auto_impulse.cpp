/*
 * Animatronic Eyes
 * Copyright (c) 2025 Zappo-II
 * Licensed under CC BY-NC-SA 4.0
 * https://github.com/Zappo-II/animatronic-eyes
 */

#include "auto_impulse.h"
#include "impulse_player.h"
#include "web_server.h"
#include "storage.h"

AutoImpulse autoImpulse;

void AutoImpulse::begin() {
    // Load config from storage
    ImpulseConfig config = storage.getImpulseConfig();
    _enabled = config.autoImpulse;
    _intervalMin = config.impulseIntervalMin;
    _intervalMax = config.impulseIntervalMax;
    strncpy(_selection, config.impulseSelection, sizeof(_selection) - 1);
    _selection[sizeof(_selection) - 1] = '\0';

    scheduleNextImpulse();

    // Preload first impulse from selection
    preloadFromSelection();
}

void AutoImpulse::loop() {
    if (!isActive()) return;

    // Don't trigger if impulse player is busy
    if (impulsePlayer.isPlaying() || impulsePlayer.isPending()) return;

    // Ensure we have a preloaded impulse (recovery after mode switch)
    if (!impulsePlayer.isPreloaded()) {
        preloadFromSelection();
    }

    if (millis() >= _nextImpulseTime) {
        // Trigger the preloaded impulse (preload happens in stopPlayback)
        WEB_LOG("AutoImpulse", "Auto-triggered impulse");
        impulsePlayer.trigger();

        scheduleNextImpulse();
    }
}

bool AutoImpulse::isActive() const {
    // Paused takes priority (e.g., during calibration)
    if (_paused) return false;

    // Runtime override takes priority over config setting
    if (_hasRuntimeOverride) return _runtimeOverride;

    // Must be enabled
    if (!_enabled) return false;

    // Must have at least one impulse selected
    if (getSelectedCount() == 0) return false;

    return true;
}

void AutoImpulse::setEnabled(bool enabled) {
    _enabled = enabled;
    if (enabled) {
        scheduleNextImpulse();
        preloadFromSelection();
    }
}

void AutoImpulse::setInterval(uint32_t minMs, uint32_t maxMs) {
    _intervalMin = minMs;
    _intervalMax = maxMs;
    // Ensure min <= max
    if (_intervalMin > _intervalMax) {
        _intervalMin = _intervalMax;
    }
}

void AutoImpulse::setSelection(const char* selectionCsv) {
    strncpy(_selection, selectionCsv, sizeof(_selection) - 1);
    _selection[sizeof(_selection) - 1] = '\0';

    // Preload a new impulse from the updated selection
    preloadFromSelection();
}

bool AutoImpulse::isImpulseSelected(const char* impulseName) const {
    if (_selection[0] == '\0') return false;

    // Parse comma-separated list
    char buffer[IMPULSE_SELECTION_STRLEN];
    strncpy(buffer, _selection, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    char* token = strtok(buffer, ",");
    while (token != nullptr) {
        // Trim whitespace
        while (*token == ' ') token++;
        char* end = token + strlen(token) - 1;
        while (end > token && *end == ' ') *end-- = '\0';

        if (strcmp(token, impulseName) == 0) {
            return true;
        }
        token = strtok(nullptr, ",");
    }
    return false;
}

int AutoImpulse::getSelectedCount() const {
    if (_selection[0] == '\0') return 0;

    int count = 0;
    char buffer[IMPULSE_SELECTION_STRLEN];
    strncpy(buffer, _selection, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    char* token = strtok(buffer, ",");
    while (token != nullptr) {
        // Trim whitespace
        while (*token == ' ') token++;
        if (*token != '\0') {
            count++;
        }
        token = strtok(nullptr, ",");
    }
    return count;
}

void AutoImpulse::resetTimer() {
    // Call this after a manual impulse to avoid immediate auto-impulse
    scheduleNextImpulse();
}

void AutoImpulse::scheduleNextImpulse() {
    unsigned long interval = random(_intervalMin, _intervalMax + 1);
    _nextImpulseTime = millis() + interval;
}

void AutoImpulse::preloadFromSelection() {
    int count = getSelectedCount();
    if (count == 0) return;

    // Build array of selected impulse names
    char buffer[IMPULSE_SELECTION_STRLEN];
    strncpy(buffer, _selection, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    // Count and collect names
    char* names[MAX_IMPULSE_SELECTION];
    int n = 0;

    char* token = strtok(buffer, ",");
    while (token != nullptr && n < MAX_IMPULSE_SELECTION) {
        // Trim whitespace
        while (*token == ' ') token++;
        char* end = token + strlen(token) - 1;
        while (end > token && *end == ' ') *end-- = '\0';

        if (*token != '\0') {
            names[n++] = token;
        }
        token = strtok(nullptr, ",");
    }

    if (n == 0) return;

    // Select random from the list
    int index = random(n);
    impulsePlayer.preloadByName(names[index]);
}

bool AutoImpulse::selectRandomFromSelection(char* buffer, size_t bufferSize) {
    int count = getSelectedCount();
    if (count == 0) return false;

    // Build array of selected impulse names
    char selBuf[IMPULSE_SELECTION_STRLEN];
    strncpy(selBuf, _selection, sizeof(selBuf) - 1);
    selBuf[sizeof(selBuf) - 1] = '\0';

    char* names[MAX_IMPULSE_SELECTION];
    int n = 0;

    char* token = strtok(selBuf, ",");
    while (token != nullptr && n < MAX_IMPULSE_SELECTION) {
        // Trim whitespace
        while (*token == ' ') token++;
        char* end = token + strlen(token) - 1;
        while (end > token && *end == ' ') *end-- = '\0';

        if (*token != '\0') {
            names[n++] = token;
        }
        token = strtok(nullptr, ",");
    }

    if (n == 0) return false;

    // Select random
    int index = random(n);
    strncpy(buffer, names[index], bufferSize - 1);
    buffer[bufferSize - 1] = '\0';
    return true;
}
