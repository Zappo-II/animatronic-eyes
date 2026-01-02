/*
 * Animatronic Eyes
 * Copyright (c) 2025 Zappo-II
 * Licensed under CC BY-NC-SA 4.0
 * https://github.com/Zappo-II/animatronic-eyes
 */

#ifndef AUTO_BLINK_H
#define AUTO_BLINK_H

#include <Arduino.h>

// Auto-Blink - Periodic automatic blinking
// Runs in both Follow and Auto modes for natural eye behavior
// Uses non-blocking async blink from Eye Controller

class AutoBlink {
public:
    void begin();
    void loop();

    // Enable/disable auto-blink (persistent setting)
    void setEnabled(bool enabled);
    bool isEnabled() const { return _enabled; }

    // Pause/resume (temporary, doesn't affect config)
    // Use for calibration mode or temporary override
    void pause() { _paused = true; }
    void resume() { _paused = false; scheduleNextBlink(); }
    bool isPaused() const { return _paused; }

    // Runtime override (temporary toggle, doesn't affect config)
    void setRuntimeOverride(bool enabled) { _runtimeOverride = enabled; _hasRuntimeOverride = true; }
    void clearRuntimeOverride() { _hasRuntimeOverride = false; }
    bool hasRuntimeOverride() const { return _hasRuntimeOverride; }
    bool getRuntimeOverride() const { return _runtimeOverride; }

    // Effective state (considers enabled, paused, and override)
    bool isActive() const;

    // Configure blink interval range (milliseconds)
    void setInterval(uint16_t minMs, uint16_t maxMs);
    uint16_t getIntervalMin() const { return _intervalMin; }
    uint16_t getIntervalMax() const { return _intervalMax; }

    // Reset timer (call after manual blink to avoid double-blink)
    void resetTimer();

private:
    bool _enabled = true;
    bool _paused = false;
    bool _hasRuntimeOverride = false;
    bool _runtimeOverride = false;
    uint16_t _intervalMin = 2000;   // 2 seconds minimum
    uint16_t _intervalMax = 6000;   // 6 seconds maximum
    unsigned long _nextBlinkTime = 0;

    void scheduleNextBlink();
};

extern AutoBlink autoBlink;

#endif // AUTO_BLINK_H
