/*
 * Animatronic Eyes
 * Copyright (c) 2025 Zappo-II
 * Licensed under CC BY-NC-SA 4.0
 * https://github.com/Zappo-II/animatronic-eyes
 */

#ifndef AUTO_IMPULSE_H
#define AUTO_IMPULSE_H

#include <Arduino.h>
#include "storage.h"

// Auto-Impulse - Periodic automatic impulse triggering
// Runs in all modes (Follow and Auto), can be toggled locally in Follow mode
// Selects from configured impulse selection list

class AutoImpulse {
public:
    void begin();
    void loop();

    // Enable/disable auto-impulse (persistent setting)
    void setEnabled(bool enabled);
    bool isEnabled() const { return _enabled; }

    // Pause/resume (temporary, doesn't affect config)
    // Use for calibration mode or temporary override
    void pause() { _paused = true; }
    void resume() { _paused = false; scheduleNextImpulse(); }
    bool isPaused() const { return _paused; }

    // Runtime override (temporary toggle, doesn't affect config)
    void setRuntimeOverride(bool enabled) { _runtimeOverride = enabled; _hasRuntimeOverride = true; }
    void clearRuntimeOverride() { _hasRuntimeOverride = false; }
    bool hasRuntimeOverride() const { return _hasRuntimeOverride; }
    bool getRuntimeOverride() const { return _runtimeOverride; }

    // Effective state (considers enabled, paused, and selection)
    bool isActive() const;

    // Configure impulse interval range (milliseconds)
    void setInterval(uint32_t minMs, uint32_t maxMs);
    uint32_t getIntervalMin() const { return _intervalMin; }
    uint32_t getIntervalMax() const { return _intervalMax; }

    // Selection management
    void setSelection(const char* selectionCsv);
    const char* getSelection() const { return _selection; }
    bool isImpulseSelected(const char* impulseName) const;
    int getSelectedCount() const;

    // Reset timer (call after manual impulse to avoid double-trigger)
    void resetTimer();

    // Preload a random impulse from selection for instant trigger
    void preloadFromSelection();

private:
    bool _enabled = true;
    bool _paused = false;
    bool _hasRuntimeOverride = false;
    bool _runtimeOverride = false;
    uint32_t _intervalMin = 30000;    // 30 seconds minimum
    uint32_t _intervalMax = 120000;   // 2 minutes maximum
    unsigned long _nextImpulseTime = 0;
    char _selection[IMPULSE_SELECTION_STRLEN] = "";

    void scheduleNextImpulse();
    bool selectRandomFromSelection(char* buffer, size_t bufferSize);
};

extern AutoImpulse autoImpulse;

#endif // AUTO_IMPULSE_H
