/*
 * Animatronic Eyes
 * Copyright (c) 2025 Zappo-II
 * Licensed under CC BY-NC-SA 4.0
 * https://github.com/Zappo-II/animatronic-eyes
 */

#include "auto_blink.h"
#include "eye_controller.h"
#include "impulse_player.h"
#include "storage.h"
#include "web_server.h"

AutoBlink autoBlink;

void AutoBlink::begin() {
    // Load saved config from storage
    ModeConfig config = storage.getModeConfig();
    _enabled = config.autoBlink;
    _intervalMin = config.blinkIntervalMin;
    _intervalMax = config.blinkIntervalMax;

    scheduleNextBlink();
}

void AutoBlink::loop() {
    if (!isActive()) return;

    // Don't trigger if eye controller is busy with another animation
    if (eyeController.isAnimating()) return;

    // Skip blink during impulse (impulse has precedence)
    if (impulsePlayer.isPlaying() || impulsePlayer.isPending()) return;

    if (millis() >= _nextBlinkTime) {
        WEB_LOG("AutoBlink", "Auto-triggered blink");
        eyeController.startBlink(0);  // 0 = scaled duration based on lid position
        scheduleNextBlink();
    }
}

bool AutoBlink::isActive() const {
    // Paused takes priority (e.g., during calibration)
    if (_paused) return false;

    // Runtime override takes priority over config setting
    if (_hasRuntimeOverride) return _runtimeOverride;

    // Fall back to config setting
    return _enabled;
}

void AutoBlink::setEnabled(bool enabled) {
    _enabled = enabled;
    if (enabled) {
        scheduleNextBlink();
    }
}

void AutoBlink::setInterval(uint16_t minMs, uint16_t maxMs) {
    _intervalMin = minMs;
    _intervalMax = maxMs;
    // Ensure min <= max
    if (_intervalMin > _intervalMax) {
        _intervalMin = _intervalMax;
    }
}

void AutoBlink::resetTimer() {
    // Call this after a manual blink to avoid immediate auto-blink
    scheduleNextBlink();
}

void AutoBlink::scheduleNextBlink() {
    unsigned long interval = random(_intervalMin, _intervalMax + 1);
    _nextBlinkTime = millis() + interval;
}
