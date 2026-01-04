/*
 * Animatronic Eyes
 * Copyright (c) 2025 Zappo-II
 * Licensed under CC BY-NC-SA 4.0
 * https://github.com/Zappo-II/animatronic-eyes
 */

#include "servo_controller.h"
#include "web_server.h"
#include <ESP32Servo.h>

ServoController servoController;

static Servo servos[NUM_SERVOS];

static const char* SERVO_NAMES[NUM_SERVOS] = {
    "Left Eye X",
    "Left Eye Y",
    "Left Eyelid",
    "Right Eye X",
    "Right Eye Y",
    "Right Eyelid"
};

void ServoController::begin() {
    // Allow allocation of all timers for servo library
    ESP32PWM::allocateTimer(0);
    ESP32PWM::allocateTimer(1);
    ESP32PWM::allocateTimer(2);
    ESP32PWM::allocateTimer(3);

    for (uint8_t i = 0; i < NUM_SERVOS; i++) {
        _configs[i] = storage.getServoConfig(i);
        _positions[i] = _configs[i].center;
        _targetPositions[i] = _configs[i].center;

        servos[i].setPeriodHertz(50);  // Standard 50Hz servo
        int result = servos[i].attach(_configs[i].pin, 500, 2400);
        if (result < 0) {
            WEB_LOG("Servo", "ERROR: Failed to attach %s on pin %d", SERVO_NAMES[i], _configs[i].pin);
        } else {
            uint8_t startPos = applyInvert(i, _configs[i].center);
            servos[i].write(startPos);
        }
    }
}

void ServoController::loop() {
    // Handle deferred centerAll request
    if (_centerAllRequested) {
        _centerAllRequested = false;
        // Set all targets to center - don't call centerAll() to avoid any issues
        for (uint8_t i = 0; i < NUM_SERVOS; i++) {
            _targetPositions[i] = _configs[i].center;
        }
    }

    // IMPORTANT: Servo writes MUST be throttled to prevent Task Watchdog Timer crashes.
    // ESP32Servo's write() blocks briefly for PWM updates. Rapid successive writes
    // (e.g., centerAll writing 6 servos instantly) starve the RTOS task scheduler,
    // triggering TWDT reset. Solution: write paired servos together for synchronized
    // movement while still preventing watchdog issues. (Fixed in v0.2.16, improved in v0.7.5)
    static unsigned long lastServoUpdate = 0;

    if (millis() - lastServoUpdate >= SERVO_UPDATE_INTERVAL_MS) {
        // Servo pairs that should be written together for synchronized movement:
        // - Left/Right eyelids (indices 2, 5)
        // - Left/Right eye X (indices 0, 3)
        // - Left/Right eye Y (indices 1, 4)
        static const uint8_t PAIRS[3][2] = {
            {SERVO_LEFT_EYELID, SERVO_RIGHT_EYELID},  // Eyelids: most important for blink sync
            {SERVO_LEFT_EYE_X, SERVO_RIGHT_EYE_X},    // Eye X: horizontal gaze
            {SERVO_LEFT_EYE_Y, SERVO_RIGHT_EYE_Y}     // Eye Y: vertical gaze
        };

        bool wroteAnything = false;

        // Check each pair - if either servo has a pending change, write both
        for (uint8_t p = 0; p < 3; p++) {
            uint8_t s1 = PAIRS[p][0];
            uint8_t s2 = PAIRS[p][1];

            bool s1Pending = (_positions[s1] != _targetPositions[s1]);
            bool s2Pending = (_positions[s2] != _targetPositions[s2]);

            if (s1Pending || s2Pending) {
                // Write both servos of the pair together
                _positions[s1] = _targetPositions[s1];
                _positions[s2] = _targetPositions[s2];
                servos[s1].write(applyInvert(s1, _positions[s1]));
                servos[s2].write(applyInvert(s2, _positions[s2]));
                wroteAnything = true;
                break;  // Only one pair per tick to avoid watchdog
            }
        }

        if (wroteAnything) {
            lastServoUpdate = millis();
        }
    }
}

void ServoController::setPosition(uint8_t index, uint8_t position) {
    if (index >= NUM_SERVOS) return;

    uint8_t constrained = constrainToCalibration(index, position);
    _targetPositions[index] = constrained;
}

void ServoController::setPositionRaw(uint8_t index, uint8_t position) {
    if (index >= NUM_SERVOS) return;

    // Constrain to servo physical limits only, not calibration
    uint8_t constrained = constrain(position, 0, 180);
    _targetPositions[index] = constrained;
}

uint8_t ServoController::getPosition(uint8_t index) {
    if (index >= NUM_SERVOS) return 90;
    return _positions[index];
}

void ServoController::requestCenterAll() {
    _centerAllRequested = true;
}

void ServoController::centerAll() {
    for (uint8_t i = 0; i < NUM_SERVOS; i++) {
        center(i);
    }
}

void ServoController::center(uint8_t index) {
    if (index >= NUM_SERVOS) return;
    setPosition(index, _configs[index].center);
}

const ServoConfig& ServoController::getConfig(uint8_t index) {
    static const ServoConfig empty = {0, 0, 90, 180, false};
    if (index >= NUM_SERVOS) {
        return empty;
    }
    return _configs[index];
}

void ServoController::setPin(uint8_t index, uint8_t pin) {
    if (index >= NUM_SERVOS) return;

    _configs[index].pin = pin;
    storage.setServoPin(index, pin);
    reattach(index);
}

void ServoController::setCalibration(uint8_t index, uint8_t min, uint8_t center, uint8_t max) {
    if (index >= NUM_SERVOS) return;

    _configs[index].min = min;
    _configs[index].center = center;
    _configs[index].max = max;
    storage.setServoCalibration(index, min, center, max);
}

void ServoController::setInvert(uint8_t index, bool invert) {
    if (index >= NUM_SERVOS) return;

    _configs[index].invert = invert;
    storage.setServoInvert(index, invert);

    // Move to center position for safety - avoids dangerous jump to mirrored position
    // which could damage mechanical linkages if servo was near an extreme
    uint8_t centerPos = _configs[index].center;
    _positions[index] = centerPos;
    _targetPositions[index] = centerPos;
    uint8_t actualPos = applyInvert(index, centerPos);
    servos[index].write(actualPos);
}

void ServoController::reattach(uint8_t index) {
    if (index >= NUM_SERVOS) return;

    servos[index].detach();
    delay(50);

    servos[index].setPeriodHertz(50);
    int result = servos[index].attach(_configs[index].pin, 500, 2400);
    if (result < 0) {
        WEB_LOG("Servo", "ERROR: Failed to reattach %s on pin %d", SERVO_NAMES[index], _configs[index].pin);
    } else {
        uint8_t actualPos = applyInvert(index, _positions[index]);
        servos[index].write(actualPos);
    }
}

uint8_t ServoController::constrainToCalibration(uint8_t index, uint8_t position) {
    return constrain(position, _configs[index].min, _configs[index].max);
}

uint8_t ServoController::applyInvert(uint8_t index, uint8_t position) {
    if (_configs[index].invert) {
        return 180 - position;
    }
    return position;
}
