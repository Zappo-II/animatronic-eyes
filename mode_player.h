/*
 * Animatronic Eyes
 * Copyright (c) 2025 Zappo-II
 * Licensed under CC BY-NC-SA 4.0
 * https://github.com/Zappo-II/animatronic-eyes
 */

#ifndef MODE_PLAYER_H
#define MODE_PLAYER_H

#include <Arduino.h>
#include <ArduinoJson.h>

// Mode Player - JSON sequence executor
// Loads mode definitions from /modes/*.json and executes them
// Supports primitives: gaze, lids, blink, wait
// Supports random values and looping sequences

class ModePlayer {
public:
    // Load a mode from /modes/<modeName>.json
    bool loadMode(const char* modeName);
    void unload();

    // Playback control
    void start();
    void stop();
    void loop();

    // Pause/resume (for calibration mode)
    void pause() { _paused = true; }
    void resume() { _paused = false; }
    bool isPaused() const { return _paused; }

    // State queries
    bool isPlaying() const { return _playing && !_paused; }
    bool isLoaded() const { return _loaded; }
    const char* getModeName() const { return _modeName; }

private:
    bool _loaded = false;
    bool _playing = false;
    bool _paused = false;
    bool _loop = true;
    char _modeName[32] = "";

    // JSON document for mode storage
    JsonDocument _modeDoc;
    JsonArray _sequence;
    int _stepCount = 0;
    int _currentStep = 0;

    // Execution state
    bool _waitingForAnimation = false;
    unsigned long _waitUntil = 0;

    // Mode parameters (from JSON)
    float _coupling = 1.0;  // Can override coupling per mode

    // Step execution
    void executeStep(JsonObject step);
    void advanceStep();

    // Primitive executors
    void execGaze(JsonObject params);
    void execLids(JsonObject params);
    void execBlink(JsonVariant params);
    void execWait(JsonVariant params);

    // Value resolution (handles random ranges)
    float resolveValue(JsonVariant val, float defaultVal = 0);
    int resolveIntValue(JsonVariant val, int defaultVal = 0);
};

extern ModePlayer modePlayer;

#endif // MODE_PLAYER_H
