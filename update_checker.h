/*
 * Animatronic Eyes
 * Copyright (c) 2025 Zappo-II
 * Licensed under CC BY-NC-SA 4.0
 * https://github.com/Zappo-II/animatronic-eyes
 */

#ifndef UPDATE_CHECKER_H
#define UPDATE_CHECKER_H

#include <Arduino.h>
#include "config.h"
#include "storage.h"

class UpdateChecker {
public:
    void begin();
    void loop();

    // Manual trigger (always allowed, not admin-protected)
    void checkNow();

    // State getters
    bool isUpdateAvailable() const;
    const char* getAvailableVersion() const;
    unsigned long getLastCheckTime() const;
    bool isCheckInProgress() const;

    // Config
    void setEnabled(bool enabled);
    void setInterval(uint8_t interval);
    bool isEnabled() const;
    uint8_t getInterval() const;

private:
    bool _updateAvailable = false;
    char _availableVersion[16] = "";
    unsigned long _lastCheckTime = 0;
    unsigned long _nextCheckTime = 0;
    bool _checkInProgress = false;
    bool _bootCheckDone = false;

    // Cached config
    bool _enabled = true;
    uint8_t _interval = 1;

    void performCheck();
    void scheduleNextCheck();
    bool shouldCheck();
    unsigned long getIntervalMs() const;
    bool isNewerVersion(const char* remote, const char* local);
};

extern UpdateChecker updateChecker;

#endif // UPDATE_CHECKER_H
