/*
 * Animatronic Eyes
 * Copyright (c) 2025 Zappo-II
 * Licensed under CC BY-NC-SA 4.0
 * https://github.com/Zappo-II/animatronic-eyes
 */

#include "mode_manager.h"
#include "mode_player.h"
#include "eye_controller.h"
#include "auto_blink.h"
#include "auto_impulse.h"
#include "storage.h"
#include "web_server.h"
#include <LittleFS.h>

extern AutoImpulse autoImpulse;

ModeManager modeManager;

void ModeManager::begin() {
    // Load mode config - defaultMode is always the startup mode
    ModeConfig config = storage.getModeConfig();

    WEB_LOG("Mode", "Starting in mode: %s", config.defaultMode);

    if (strcmp(config.defaultMode, "follow") == 0) {
        if (!setMode(Mode::FOLLOW)) {
            setError("Failed to enter Follow mode");
            enterNoneMode();
        }
    } else {
        // Try to load the configured auto mode
        if (!setAutoMode(config.defaultMode)) {
            WEB_LOG("Mode", "Failed to load mode '%s', falling back to Follow", config.defaultMode);
            if (!setMode(Mode::FOLLOW)) {
                setError("Failed to enter Follow mode");
                enterNoneMode();
            }
        }
    }
}

void ModeManager::loop() {
    if (_currentMode == Mode::AUTO) {
        modePlayer.loop();

        // Check if mode player stopped (non-looping mode finished)
        if (!modePlayer.isPlaying()) {
            WEB_LOG("Mode", "Auto mode '%s' finished, switching to Follow", _currentAutoModeName);
            setMode(Mode::FOLLOW);
        }
    }
}

const char* ModeManager::getCurrentModeName() const {
    switch (_currentMode) {
        case Mode::NONE:   return "none";
        case Mode::FOLLOW: return "follow";
        case Mode::AUTO:   return _currentAutoModeName;
        default:           return "unknown";
    }
}

bool ModeManager::setMode(Mode mode) {
    if (mode == _currentMode && mode != Mode::AUTO) return true;

    // AUTO mode must use setAutoMode() to specify which mode
    if (mode == Mode::AUTO) {
        return false;
    }

    exitCurrentMode();

    switch (mode) {
        case Mode::NONE:
            enterNoneMode();
            break;
        case Mode::FOLLOW:
            enterFollowMode();
            // If "remember last mode" is enabled, update defaultMode
            {
                ModeConfig config = storage.getModeConfig();
                if (config.rememberLastMode) {
                    strncpy(config.defaultMode, "follow", sizeof(config.defaultMode) - 1);
                    storage.setModeConfig(config);
                }
            }
            break;
        default:
            return false;
    }

    _currentMode = mode;
    clearError();
    return true;
}

bool ModeManager::setAutoMode(const char* modeName) {
    if (!modePlayer.loadMode(modeName)) {
        setError("Failed to load mode");
        return false;
    }

    exitCurrentMode();

    _currentMode = Mode::AUTO;
    strncpy(_currentAutoModeName, modeName, sizeof(_currentAutoModeName) - 1);
    _currentAutoModeName[sizeof(_currentAutoModeName) - 1] = '\0';

    // If "remember last mode" is enabled, update defaultMode
    {
        ModeConfig config = storage.getModeConfig();
        if (config.rememberLastMode) {
            strncpy(config.defaultMode, modeName, sizeof(config.defaultMode) - 1);
            storage.setModeConfig(config);
        }
    }

    modePlayer.start();
    clearError();

    WEB_LOG("Mode", "Entered AUTO mode: %s", modeName);
    return true;
}

void ModeManager::enterNoneMode() {
    // Safe state: full reset, no movement
    eyeController.resetAll();
    _currentAutoModeName[0] = '\0';
    WEB_LOG("Mode", "Entered NONE mode (safe state)");
}

void ModeManager::enterFollowMode() {
    _currentAutoModeName[0] = '\0';
    WEB_LOG("Mode", "Entered FOLLOW mode");
}

void ModeManager::exitCurrentMode() {
    if (_currentMode == Mode::AUTO) {
        modePlayer.stop();
    }
    _currentAutoModeName[0] = '\0';

    // Full reset including Z and coupling when switching modes
    eyeController.resetAll();

    // Clear any runtime overrides when changing modes
    autoBlink.clearRuntimeOverride();
    autoImpulse.clearRuntimeOverride();
}

int ModeManager::getAvailableModeCount() {
    int count = 0;
    File root = LittleFS.open("/modes");
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

bool ModeManager::getAvailableModeName(int index, char* buffer, size_t bufferSize) {
    int count = 0;
    File root = LittleFS.open("/modes");
    if (!root || !root.isDirectory()) {
        return false;
    }

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

void ModeManager::setError(const char* message) {
    _hasError = true;
    strncpy(_errorMessage, message, sizeof(_errorMessage) - 1);
    _errorMessage[sizeof(_errorMessage) - 1] = '\0';
}

void ModeManager::clearError() {
    _hasError = false;
    _errorMessage[0] = '\0';
}
