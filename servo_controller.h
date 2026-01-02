/*
 * Animatronic Eyes
 * Copyright (c) 2025 Zappo-II
 * Licensed under CC BY-NC-SA 4.0
 * https://github.com/Zappo-II/animatronic-eyes
 */

#ifndef SERVO_CONTROLLER_H
#define SERVO_CONTROLLER_H

#include <Arduino.h>
#include "config.h"
#include "storage.h"

class ServoController {
public:
    void begin();
    void loop();

    // Position control (0-180, will be constrained to calibration limits)
    void setPosition(uint8_t index, uint8_t position);
    void setPositionRaw(uint8_t index, uint8_t position);  // Bypasses calibration limits (for calibration preview)
    uint8_t getPosition(uint8_t index);

    // Move to center position
    void requestCenterAll();  // Safe to call from async context (deferred to loop)
    void center(uint8_t index);

    // Configuration
    const ServoConfig& getConfig(uint8_t index);
    void setPin(uint8_t index, uint8_t pin);
    void setCalibration(uint8_t index, uint8_t min, uint8_t center, uint8_t max);
    void setInvert(uint8_t index, bool invert);

    // Detach/reattach (for pin changes)
    void reattach(uint8_t index);

private:
    ServoConfig _configs[NUM_SERVOS];
    uint8_t _positions[NUM_SERVOS];
    uint8_t _targetPositions[NUM_SERVOS];
    volatile bool _centerAllRequested = false;

    void centerAll();  // Private - executed in loop()
    uint8_t constrainToCalibration(uint8_t index, uint8_t position);
    uint8_t applyInvert(uint8_t index, uint8_t position);
};

extern ServoController servoController;

#endif // SERVO_CONTROLLER_H
