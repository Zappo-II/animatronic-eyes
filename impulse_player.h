/*
 * Animatronic Eyes
 * Copyright (c) 2025 Zappo-II
 * Licensed under CC BY-NC-SA 4.0
 * https://github.com/Zappo-II/animatronic-eyes
 */

#ifndef IMPULSE_PLAYER_H
#define IMPULSE_PLAYER_H

#include <Arduino.h>
#include <ArduinoJson.h>

// Impulse Player - One-shot animation sequences with state restore
// Loads impulse definitions from /impulses/*.json and executes them
// Saves eye state before playing, restores after completion
// Supports primitives: gaze, lids, blink, wait (same as modes)

class ImpulsePlayer {
public:
    void begin();
    void loop();

    // Trigger an impulse
    // If preloaded impulse is available, plays it immediately
    // Otherwise loads from file (may have brief delay)
    bool trigger();

    // Trigger a specific impulse by name
    bool triggerByName(const char* impulseName);

    // Preload system - loads impulse into memory for instant trigger
    bool preloadByName(const char* impulseName);
    bool isPreloaded() const { return _preloaded; }
    const char* getPreloadedName() const { return _preloadedName; }

    // State queries
    bool isPlaying() const { return _playing; }
    bool isPending() const { return _pending; }
    const char* getCurrentImpulseName() const { return _currentImpulseName; }

    // Stop current impulse (restores state, preloads next)
    void stop();

    // Available impulses (from /impulses/ directory)
    int getAvailableImpulseCount();
    bool getAvailableImpulseName(int index, char* buffer, size_t bufferSize);

private:
    // Preloaded impulse (ready for instant trigger)
    bool _preloaded = false;
    char _preloadedName[32] = "";
    JsonDocument _preloadedDoc;
    JsonArray _preloadedSequence;
    int _preloadedStepCount = 0;

    // Current playback state
    bool _playing = false;
    bool _pending = false;  // Waiting for blink to finish
    char _currentImpulseName[32] = "";

    // Playback from preloaded or freshly loaded
    JsonDocument _playDoc;  // Only used when not playing from preloaded
    JsonArray _sequence;
    int _stepCount = 0;
    int _currentStep = 0;

    // Execution state
    bool _waitingForAnimation = false;
    unsigned long _waitUntil = 0;

    // Saved state for restore (static allocation, ~28 bytes)
    struct SavedState {
        float gazeX;
        float gazeY;
        float gazeZ;
        float coupling;
        float lidLeft;
        float lidRight;
    };
    SavedState _savedState;

    // State management
    void saveState();
    void restoreState();

    // Playback control
    bool startPlayback();
    void stopPlayback();

    // Load impulse from file into specified document
    bool loadImpulse(const char* impulseName, JsonDocument& doc, JsonArray& sequence, int& stepCount);

    // Step execution (reuses same logic as mode_player)
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

extern ImpulsePlayer impulsePlayer;

#endif // IMPULSE_PLAYER_H
