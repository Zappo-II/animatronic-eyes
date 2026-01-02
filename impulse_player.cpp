/*
 * Animatronic Eyes
 * Copyright (c) 2025 Zappo-II
 * Licensed under CC BY-NC-SA 4.0
 * https://github.com/Zappo-II/animatronic-eyes
 */

#include "impulse_player.h"
#include "eye_controller.h"
#include "auto_blink.h"
#include "auto_impulse.h"
#include "web_server.h"
#include <LittleFS.h>

ImpulsePlayer impulsePlayer;

void ImpulsePlayer::begin() {
    // Initial preload handled by autoImpulse.begin()
}

void ImpulsePlayer::loop() {
    // Handle pending state (waiting for blink to finish before starting)
    if (_pending) {
        if (!eyeController.isAnimating()) {
            _pending = false;
            startPlayback();
        }
        return;
    }

    if (!_playing) return;

    // Check if waiting for a timed delay
    if (_waitUntil > 0) {
        if (millis() < _waitUntil) {
            return;  // Still waiting
        }
        _waitUntil = 0;
        advanceStep();  // Wait complete, move to next step
        return;  // Execute next step on next loop iteration
    }

    // Check if waiting for animation (blink) to complete
    if (_waitingForAnimation) {
        if (eyeController.isAnimating()) {
            return;  // Still animating
        }
        _waitingForAnimation = false;
        advanceStep();  // Animation complete, move to next step
        return;  // Execute next step on next loop iteration
    }

    // Execute current step
    if (_currentStep < _stepCount) {
        JsonObject step = _sequence[_currentStep].as<JsonObject>();
        executeStep(step);

        // Advance immediately only if not waiting
        // (wait/blink steps set flags and will advance when complete)
        if (_waitUntil == 0 && !_waitingForAnimation) {
            advanceStep();
        }
    }
}

bool ImpulsePlayer::trigger() {
    // If already playing, ignore
    if (_playing || _pending) return false;

    // If preloaded, use it
    if (_preloaded) {
        // Mark as consumed (sequence still valid until we preload next)
        _preloaded = false;

        // Copy preloaded name to current
        strncpy(_currentImpulseName, _preloadedName, sizeof(_currentImpulseName) - 1);
        _currentImpulseName[sizeof(_currentImpulseName) - 1] = '\0';

        // Use preloaded sequence directly
        _sequence = _preloadedSequence;
        _stepCount = _preloadedStepCount;

        // Check if we need to wait for blink to finish
        if (eyeController.isAnimating()) {
            _pending = true;
            WEB_LOG("Impulse", "Impulse '%s' pending (waiting for animation)", _currentImpulseName);
            return true;
        }

        // Start immediately (preload next after playback completes in stopPlayback)
        return startPlayback();
    }

    // No preloaded impulse - this shouldn't happen normally
    WEB_LOG("Impulse", "No preloaded impulse available");
    return false;
}

bool ImpulsePlayer::triggerByName(const char* impulseName) {
    // If already playing, ignore
    if (_playing || _pending) return false;

    // Check if this is the preloaded impulse
    if (_preloaded && strcmp(_preloadedName, impulseName) == 0) {
        return trigger();  // Use the preloaded one
    }

    // Load the requested impulse
    if (!loadImpulse(impulseName, _playDoc, _sequence, _stepCount)) {
        return false;
    }

    strncpy(_currentImpulseName, impulseName, sizeof(_currentImpulseName) - 1);
    _currentImpulseName[sizeof(_currentImpulseName) - 1] = '\0';

    // Check if we need to wait for blink to finish
    if (eyeController.isAnimating()) {
        _pending = true;
        WEB_LOG("Impulse", "Impulse '%s' pending (waiting for animation)", _currentImpulseName);
        return true;
    }

    return startPlayback();
}

bool ImpulsePlayer::startPlayback() {
    if (_stepCount == 0) return false;

    // Save current eye state
    saveState();

    _currentStep = 0;
    _playing = true;
    _waitingForAnimation = false;
    _waitUntil = 0;

    WEB_LOG("Impulse", "Playing '%s' (%d steps)", _currentImpulseName, _stepCount);
    return true;
}

void ImpulsePlayer::stop() {
    stopPlayback();
}

void ImpulsePlayer::stopPlayback() {
    if (_playing) {
        // Restore saved state
        restoreState();

        // Reset auto-blink timer to avoid immediate blink after impulse
        autoBlink.resetTimer();

        // Preload next impulse from selection (safe now that playback is done)
        autoImpulse.preloadFromSelection();
    }

    _playing = false;
    _pending = false;
    _waitingForAnimation = false;
    _waitUntil = 0;
    _currentImpulseName[0] = '\0';
}

bool ImpulsePlayer::preloadByName(const char* impulseName) {
    _preloaded = false;
    _preloadedDoc.clear();

    if (!loadImpulse(impulseName, _preloadedDoc, _preloadedSequence, _preloadedStepCount)) {
        return false;
    }

    strncpy(_preloadedName, impulseName, sizeof(_preloadedName) - 1);
    _preloadedName[sizeof(_preloadedName) - 1] = '\0';

    _preloaded = true;
    return true;
}

bool ImpulsePlayer::loadImpulse(const char* impulseName, JsonDocument& doc, JsonArray& sequence, int& stepCount) {
    doc.clear();
    stepCount = 0;

    // Build path: /impulses/<impulseName>.json
    char path[64];
    snprintf(path, sizeof(path), "/impulses/%s.json", impulseName);

    File file = LittleFS.open(path, "r");
    if (!file) {
        WEB_LOG("Impulse", "Failed to open %s", path);
        return false;
    }

    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error) {
        WEB_LOG("Impulse", "JSON parse error: %s", error.c_str());
        return false;
    }

    // Extract sequence array
    if (!doc.containsKey("sequence")) {
        WEB_LOG("Impulse", "Impulse missing 'sequence' array");
        return false;
    }

    sequence = doc["sequence"].as<JsonArray>();
    stepCount = sequence.size();

    if (stepCount == 0) {
        WEB_LOG("Impulse", "Impulse has empty sequence");
        return false;
    }

    return true;
}

int ImpulsePlayer::getAvailableImpulseCount() {
    int count = 0;
    File root = LittleFS.open("/impulses");
    if (!root || !root.isDirectory()) {
        return 0;
    }

    File file = root.openNextFile();
    while (file) {
        if (!file.isDirectory()) {
            String name = file.name();
            if (name.endsWith(".json")) {
                count++;
            }
        }
        file = root.openNextFile();
    }
    root.close();
    return count;
}

bool ImpulsePlayer::getAvailableImpulseName(int index, char* buffer, size_t bufferSize) {
    File root = LittleFS.open("/impulses");
    if (!root || !root.isDirectory()) {
        return false;
    }

    int count = 0;
    File file = root.openNextFile();
    while (file) {
        if (!file.isDirectory()) {
            String name = file.name();
            if (name.endsWith(".json")) {
                if (count == index) {
                    // Remove .json extension
                    name = name.substring(0, name.length() - 5);
                    strncpy(buffer, name.c_str(), bufferSize - 1);
                    buffer[bufferSize - 1] = '\0';
                    root.close();
                    return true;
                }
                count++;
            }
        }
        file = root.openNextFile();
    }
    root.close();
    return false;
}

void ImpulsePlayer::saveState() {
    _savedState.gazeX = eyeController.getGazeX();
    _savedState.gazeY = eyeController.getGazeY();
    _savedState.gazeZ = eyeController.getGazeZ();
    _savedState.coupling = eyeController.getCoupling();
    _savedState.lidLeft = eyeController.getLidLeft();
    _savedState.lidRight = eyeController.getLidRight();
}

void ImpulsePlayer::restoreState() {
    eyeController.setGaze(_savedState.gazeX, _savedState.gazeY, _savedState.gazeZ);
    eyeController.setCoupling(_savedState.coupling);
    eyeController.setLids(_savedState.lidLeft, _savedState.lidRight);
}

void ImpulsePlayer::executeStep(JsonObject step) {
    // Each step can have one primitive
    if (step.containsKey("gaze")) {
        execGaze(step["gaze"].as<JsonObject>());
    }
    else if (step.containsKey("lids")) {
        execLids(step["lids"].as<JsonObject>());
    }
    else if (step.containsKey("blink")) {
        execBlink(step["blink"]);
    }
    else if (step.containsKey("wait")) {
        execWait(step["wait"]);
    }
}

void ImpulsePlayer::advanceStep() {
    _currentStep++;

    if (_currentStep >= _stepCount) {
        // Sequence complete - restore state and stop
        stopPlayback();
    }
}

void ImpulsePlayer::execGaze(JsonObject params) {
    float x = resolveValue(params["x"], eyeController.getGazeX());
    float y = resolveValue(params["y"], eyeController.getGazeY());
    float z = resolveValue(params["z"], eyeController.getGazeZ());

    eyeController.setGaze(x, y, z);
}

void ImpulsePlayer::execLids(JsonObject params) {
    float left = resolveValue(params["left"], eyeController.getLidLeft());
    float right = resolveValue(params["right"], eyeController.getLidRight());

    eyeController.setLids(left, right);
}

void ImpulsePlayer::execBlink(JsonVariant params) {
    int duration = resolveIntValue(params, 150);

    eyeController.startBlink(duration);
    autoBlink.resetTimer();  // Avoid double-blink from auto-blink

    _waitingForAnimation = true;
}

void ImpulsePlayer::execWait(JsonVariant params) {
    int ms = resolveIntValue(params, 0);

    if (ms > 0) {
        _waitUntil = millis() + ms;
    }
}

float ImpulsePlayer::resolveValue(JsonVariant val, float defaultVal) {
    if (val.isNull()) {
        return defaultVal;
    }

    // Direct numeric value
    if (val.is<float>() || val.is<int>()) {
        return val.as<float>();
    }

    // Random range: {"random": [min, max]}
    if (val.is<JsonObject>()) {
        JsonObject obj = val.as<JsonObject>();
        if (obj.containsKey("random")) {
            JsonArray range = obj["random"].as<JsonArray>();
            if (range.size() >= 2) {
                float minVal = range[0].as<float>();
                float maxVal = range[1].as<float>();
                // Generate random float in range
                return minVal + (random(10001) / 10000.0f) * (maxVal - minVal);
            }
        }
    }

    return defaultVal;
}

int ImpulsePlayer::resolveIntValue(JsonVariant val, int defaultVal) {
    if (val.isNull()) {
        return defaultVal;
    }

    // Direct numeric value
    if (val.is<int>()) {
        return val.as<int>();
    }

    // Random range: {"random": [min, max]}
    if (val.is<JsonObject>()) {
        JsonObject obj = val.as<JsonObject>();
        if (obj.containsKey("random")) {
            JsonArray range = obj["random"].as<JsonArray>();
            if (range.size() >= 2) {
                int minVal = range[0].as<int>();
                int maxVal = range[1].as<int>();
                return random(minVal, maxVal + 1);
            }
        }
    }

    return defaultVal;
}
