/*
 * Animatronic Eyes
 * Copyright (c) 2025 Zappo-II
 * Licensed under CC BY-NC-SA 4.0
 * https://github.com/Zappo-II/animatronic-eyes
 */

#ifndef MODE_MANAGER_H
#define MODE_MANAGER_H

#include <Arduino.h>

// Mode System States
// NONE: Safe/error state - eyes centered, no movement
// FOLLOW: User controls gaze via touch pad, auto-blink runs
// AUTO: JSON sequence runs autonomously, auto-blink integrated

enum class Mode { NONE, FOLLOW, AUTO };

class ModeManager {
public:
    void begin();
    void loop();

    // Current mode
    Mode getCurrentMode() const { return _currentMode; }
    const char* getCurrentModeName() const;
    const char* getCurrentAutoModeName() const { return _currentAutoModeName; }

    // Mode switching
    bool setMode(Mode mode);
    bool setAutoMode(const char* modeName);

    // Available auto modes (from /modes/ directory)
    int getAvailableModeCount();
    bool getAvailableModeName(int index, char* buffer, size_t bufferSize);

    // Error handling
    bool hasError() const { return _hasError; }
    const char* getErrorMessage() const { return _errorMessage; }
    void clearError();

private:
    Mode _currentMode = Mode::NONE;
    char _currentAutoModeName[32] = "";
    bool _hasError = false;
    char _errorMessage[64] = "";

    void enterNoneMode();
    void enterFollowMode();
    void exitCurrentMode();
    void setError(const char* message);
};

extern ModeManager modeManager;

#endif // MODE_MANAGER_H
