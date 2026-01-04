/*
 * Animatronic Eyes
 * Copyright (c) 2025 Zappo-II
 * Licensed under CC BY-NC-SA 4.0
 * https://github.com/Zappo-II/animatronic-eyes
 */

#ifndef EYE_CONTROLLER_H
#define EYE_CONTROLLER_H

#include <Arduino.h>
#include "config.h"
#include "servo_controller.h"

// Eye Controller - Abstraction layer for logical gaze/lid control
// Translates logical coordinates (-100 to +100) into calibrated servo positions
// Handles vergence (eye convergence) and coupling (linked/independent/divergent)

class EyeController {
public:
    void begin();
    void loop();

    // Gaze control (-100 to +100 for each axis)
    // X: -100 = left, 0 = center, +100 = right
    // Y: -100 = down, 0 = center, +100 = up
    // Z: -100 = very close (nose), 0 = medium, +100 = far
    void setGaze(float x, float y, float z);
    void setGazeX(float x);
    void setGazeY(float y);
    void setGazeZ(float z);

    // Eyelid control (-100 to +100)
    // -100 = fully closed, 0 = half, +100 = fully open
    void setLids(float left, float right);
    void setLeftLid(float position);
    void setRightLid(float position);

    // Blink primitives (blocking - for WebSocket commands / manual triggers)
    void blink(unsigned int durationMs = 150);
    void blinkLeft(unsigned int durationMs = 150);
    void blinkRight(unsigned int durationMs = 150);

    // Async blink primitives (non-blocking - for Mode System / Auto-blink)
    void startBlink(unsigned int durationMs = 150);
    void startBlinkLeft(unsigned int durationMs = 150);
    void startBlinkRight(unsigned int durationMs = 150);

    // Async wait (non-blocking delay for Mode System)
    void startWait(unsigned int durationMs);

    // Animation state queries
    bool isAnimating() const;
    void cancelAnimation();

    // Coupling: controls how eyes move relative to each other
    // +1.0 = fully linked with vergence (normal)
    //  0.0 = fully independent
    // -1.0 = fully divergent (Feldman mode)
    void setCoupling(float c);
    float getCoupling() const { return _coupling; }

    // Max vergence offset (how cross-eyed eyes can get at Z=-100)
    void setMaxVergence(float v);
    float getMaxVergence() const { return _maxVergence; }

    // Center gaze and lids (keeps Z and coupling)
    void center();

    // Full reset including Z and coupling (used for mode switching)
    void resetAll();

    // Re-apply current state to servos (used when returning from Calibration)
    void reapply();

    // Getters for UI feedback
    float getGazeX() const { return _gazeX; }
    float getGazeY() const { return _gazeY; }
    float getGazeZ() const { return _gazeZ; }
    float getLidLeft() const { return _lidLeft; }
    float getLidRight() const { return _lidRight; }

private:
    // Current logical state
    float _gazeX = 0;
    float _gazeY = 0;
    float _gazeZ = 0;        // Medium depth by default (vergence active)
    float _lidLeft = 0;      // Calibration center (neutral open)
    float _lidRight = 0;     // Calibration center (neutral open)

    // Parameters
    float _coupling = 1.0;      // Fully linked with vergence (normal)
    float _maxVergence = 50.0; // Max horizontal vergence offset at Z=-100
    float _maxVerticalDivergence = 50.0; // Max vertical divergence when coupling=-1 (Feldman mode)

    // === Async Animation State Machine ===
    enum class AnimState { IDLE, BLINK_CLOSING, BLINK_OPENING, WAITING };
    enum class BlinkEye { BOTH, LEFT, RIGHT };

    AnimState _animState = AnimState::IDLE;
    unsigned long _animStartTime = 0;
    unsigned long _animDuration = 0;

    // Blink-specific state
    BlinkEye _blinkEye = BlinkEye::BOTH;
    float _blinkPrevLeft = 0;
    float _blinkPrevRight = 0;

    // Calculate vergence offset based on Z depth
    float calculateVergence(float z);

    // Calculate blink duration based on lid travel distance
    unsigned int calculateBlinkDuration(float lidLeft, float lidRight);

    // Map logical coordinate (-100 to +100) to servo position
    // Uses servo's calibration (min/center/max) and invert flag
    void setServoFromLogical(uint8_t servoIndex, float logical);

    // Apply current gaze state to eye servos (X/Y with vergence)
    void applyGaze();

    // Apply current lid state to eyelid servos
    void applyLids();
};

extern EyeController eyeController;

#endif // EYE_CONTROLLER_H
