/*
 * Animatronic Eyes
 * Copyright (c) 2025 Zappo-II
 * Licensed under CC BY-NC-SA 4.0
 * https://github.com/Zappo-II/animatronic-eyes
 */

#include "mode_player.h"
#include "eye_controller.h"
#include "auto_blink.h"
#include "web_server.h"
#include <LittleFS.h>

ModePlayer modePlayer;

bool ModePlayer::loadMode(const char* modeName) {
    unload();

    // Build path: /modes/<modeName>.json
    char path[64];
    snprintf(path, sizeof(path), "/modes/%s.json", modeName);

    File file = LittleFS.open(path, "r");
    if (!file) {
        WEB_LOG("ModePlayer", "Failed to open %s", path);
        return false;
    }

    DeserializationError error = deserializeJson(_modeDoc, file);
    file.close();

    if (error) {
        WEB_LOG("ModePlayer", "JSON parse error: %s", error.c_str());
        return false;
    }

    // Extract sequence array
    if (!_modeDoc.containsKey("sequence")) {
        WEB_LOG("ModePlayer", "Mode missing 'sequence' array");
        return false;
    }

    _sequence = _modeDoc["sequence"].as<JsonArray>();
    _stepCount = _sequence.size();

    if (_stepCount == 0) {
        WEB_LOG("ModePlayer", "Mode has empty sequence");
        return false;
    }

    // Extract mode properties
    _loop = _modeDoc["loop"] | true;
    _coupling = _modeDoc["coupling"] | 1.0f;

    // Store mode name
    strncpy(_modeName, modeName, sizeof(_modeName) - 1);
    _modeName[sizeof(_modeName) - 1] = '\0';

    _loaded = true;

    WEB_LOG("ModePlayer", "Loaded '%s' with %d steps (loop=%s)",
            modeName, _stepCount, _loop ? "true" : "false");

    return true;
}

void ModePlayer::unload() {
    stop();
    _modeDoc.clear();
    _loaded = false;
    _stepCount = 0;
    _modeName[0] = '\0';
}

void ModePlayer::start() {
    if (!_loaded) return;

    _currentStep = 0;
    _playing = true;
    _paused = false;  // Clear any pause state from previous manual control
    _waitingForAnimation = false;
    _waitUntil = 0;

    // Apply mode's coupling setting
    eyeController.setCoupling(_coupling);

    WEB_LOG("ModePlayer", "Started playback of '%s'", _modeName);
}

void ModePlayer::stop() {
    _playing = false;
    _waitingForAnimation = false;
    _waitUntil = 0;

    // Reset coupling to default
    eyeController.setCoupling(1.0f);

    if (_modeName[0] != '\0') {
        WEB_LOG("ModePlayer", "Stopped playback of '%s'", _modeName);
    }
}

void ModePlayer::loop() {
    if (!_playing || !_loaded || _paused) return;

    // Check if waiting for a timed delay
    if (_waitUntil > 0) {
        if (millis() < _waitUntil) {
            return;  // Still waiting
        }
        _waitUntil = 0;
    }

    // Check if waiting for animation (blink) to complete
    if (_waitingForAnimation) {
        if (eyeController.isAnimating()) {
            return;  // Still animating
        }
        _waitingForAnimation = false;
    }

    // Execute current step
    if (_currentStep < _stepCount) {
        JsonObject step = _sequence[_currentStep].as<JsonObject>();
        executeStep(step);
        advanceStep();
    }
}

void ModePlayer::executeStep(JsonObject step) {
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

void ModePlayer::advanceStep() {
    _currentStep++;

    if (_currentStep >= _stepCount) {
        if (_loop) {
            _currentStep = 0;  // Loop back to start
        } else {
            stop();  // Sequence complete
        }
    }
}

void ModePlayer::execGaze(JsonObject params) {
    float x = resolveValue(params["x"], eyeController.getGazeX());
    float y = resolveValue(params["y"], eyeController.getGazeY());
    float z = resolveValue(params["z"], eyeController.getGazeZ());

    eyeController.setGaze(x, y, z);
}

void ModePlayer::execLids(JsonObject params) {
    // Don't override lid positions during blink animation (e.g., auto-blink)
    if (eyeController.isAnimating()) return;

    float left = resolveValue(params["left"], eyeController.getLidLeft());
    float right = resolveValue(params["right"], eyeController.getLidRight());

    eyeController.setLids(left, right);
}

void ModePlayer::execBlink(JsonVariant params) {
    int duration = resolveIntValue(params, 150);

    eyeController.startBlink(duration);
    autoBlink.resetTimer();  // Avoid double-blink from auto-blink

    _waitingForAnimation = true;
}

void ModePlayer::execWait(JsonVariant params) {
    int ms = resolveIntValue(params, 0);

    if (ms > 0) {
        _waitUntil = millis() + ms;
    }
}

float ModePlayer::resolveValue(JsonVariant val, float defaultVal) {
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

int ModePlayer::resolveIntValue(JsonVariant val, int defaultVal) {
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
