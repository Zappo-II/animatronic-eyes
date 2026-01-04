/*
 * Animatronic Eyes
 * Copyright (c) 2025 Zappo-II
 * Licensed under CC BY-NC-SA 4.0
 * https://github.com/Zappo-II/animatronic-eyes
 */

#include "eye_controller.h"
#include "servo_controller.h"
#include "web_server.h"

EyeController eyeController;

// Float version of map() - Arduino's map() only works with integers!
static float mapFloat(float x, float in_min, float in_max, float out_min, float out_max) {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void EyeController::begin() {
    // Apply initial state (centered gaze, open lids)
    applyGaze();
    applyLids();
}

void EyeController::loop() {
    // Process async animations (non-blocking)
    if (_animState == AnimState::IDLE) return;

    unsigned long elapsed = millis() - _animStartTime;

    switch (_animState) {
        case AnimState::BLINK_CLOSING:
            // Wait for half duration, then start opening
            if (elapsed >= _animDuration / 2) {
                _animState = AnimState::BLINK_OPENING;
                _animStartTime = millis();
                // Open eyes back to previous position
                switch (_blinkEye) {
                    case BlinkEye::BOTH:
                        setLids(_blinkPrevLeft, _blinkPrevRight);
                        break;
                    case BlinkEye::LEFT:
                        setLeftLid(_blinkPrevLeft);
                        break;
                    case BlinkEye::RIGHT:
                        setRightLid(_blinkPrevRight);
                        break;
                }
            }
            break;

        case AnimState::BLINK_OPENING:
            // Wait for remaining half duration, then done
            if (elapsed >= _animDuration / 2) {
                _animState = AnimState::IDLE;
            }
            break;

        case AnimState::WAITING:
            if (elapsed >= _animDuration) {
                _animState = AnimState::IDLE;
            }
            break;

        default:
            break;
    }
}

// === Gaze Control ===

void EyeController::setGaze(float x, float y, float z) {
    _gazeX = constrain(x, -100.0f, 100.0f);
    _gazeY = constrain(y, -100.0f, 100.0f);
    _gazeZ = constrain(z, -100.0f, 100.0f);
    applyGaze();
}

void EyeController::setGazeX(float x) {
    _gazeX = constrain(x, -100.0f, 100.0f);
    applyGaze();
}

void EyeController::setGazeY(float y) {
    _gazeY = constrain(y, -100.0f, 100.0f);
    applyGaze();
}

void EyeController::setGazeZ(float z) {
    _gazeZ = constrain(z, -100.0f, 100.0f);
    applyGaze();
}

// === Eyelid Control ===

void EyeController::setLids(float left, float right) {
    _lidLeft = constrain(left, -100.0f, 100.0f);
    _lidRight = constrain(right, -100.0f, 100.0f);
    applyLids();
}

void EyeController::setLeftLid(float position) {
    _lidLeft = constrain(position, -100.0f, 100.0f);
    applyLids();
}

void EyeController::setRightLid(float position) {
    _lidRight = constrain(position, -100.0f, 100.0f);
    applyLids();
}

// === Blink ===

void EyeController::blink(unsigned int durationMs) {
    // Store current lid positions
    float prevLeft = _lidLeft;
    float prevRight = _lidRight;

    // Close eyes
    setLids(-100, -100);
    delay(durationMs / 2);

    // Open eyes back to previous position
    setLids(prevLeft, prevRight);
}

void EyeController::blinkLeft(unsigned int durationMs) {
    float prevLeft = _lidLeft;
    setLeftLid(-100);
    delay(durationMs / 2);
    setLeftLid(prevLeft);
}

void EyeController::blinkRight(unsigned int durationMs) {
    float prevRight = _lidRight;
    setRightLid(-100);
    delay(durationMs / 2);
    setRightLid(prevRight);
}

// === Async Blink (non-blocking) ===

unsigned int EyeController::calculateBlinkDuration(float lidLeft, float lidRight) {
    // Calculate duration based on how far lids need to travel to close
    // Travel distance: from current position to -100 (closed)
    float travelLeft = lidLeft - (-100.0f);   // e.g., +100 to -100 = 200
    float travelRight = lidRight - (-100.0f);
    float maxTravel = max(travelLeft, travelRight);

    // Scale: base 100ms + 0.75ms per unit of travel
    // Examples:
    //   +100 (wide open): 100 + 200*0.75 = 250ms
    //   0 (neutral):      100 + 100*0.75 = 175ms
    //   -50 (half closed): 100 + 50*0.75 = 137ms
    return (unsigned int)(100 + maxTravel * 0.75f);
}

void EyeController::startBlink(unsigned int durationMs) {
    if (_animState != AnimState::IDLE) return;  // Don't interrupt ongoing animation

    _blinkPrevLeft = _lidLeft;
    _blinkPrevRight = _lidRight;
    _blinkEye = BlinkEye::BOTH;

    // If durationMs is 0, calculate scaled duration based on lid position
    _animDuration = (durationMs == 0) ? calculateBlinkDuration(_lidLeft, _lidRight) : durationMs;
    _animStartTime = millis();
    _animState = AnimState::BLINK_CLOSING;

    setLids(-100, -100);  // Close immediately
}

void EyeController::startBlinkLeft(unsigned int durationMs) {
    if (_animState != AnimState::IDLE) return;

    _blinkPrevLeft = _lidLeft;
    _blinkEye = BlinkEye::LEFT;

    // If durationMs is 0, calculate scaled duration based on lid position
    _animDuration = (durationMs == 0) ? calculateBlinkDuration(_lidLeft, -100.0f) : durationMs;
    _animStartTime = millis();
    _animState = AnimState::BLINK_CLOSING;

    setLeftLid(-100);
}

void EyeController::startBlinkRight(unsigned int durationMs) {
    if (_animState != AnimState::IDLE) return;

    _blinkPrevRight = _lidRight;
    _blinkEye = BlinkEye::RIGHT;

    // If durationMs is 0, calculate scaled duration based on lid position
    _animDuration = (durationMs == 0) ? calculateBlinkDuration(-100.0f, _lidRight) : durationMs;
    _animStartTime = millis();
    _animState = AnimState::BLINK_CLOSING;

    setRightLid(-100);
}

void EyeController::startWait(unsigned int durationMs) {
    if (_animState != AnimState::IDLE) return;

    _animDuration = durationMs;
    _animStartTime = millis();
    _animState = AnimState::WAITING;
}

bool EyeController::isAnimating() const {
    return _animState != AnimState::IDLE;
}

void EyeController::cancelAnimation() {
    if (_animState == AnimState::BLINK_CLOSING || _animState == AnimState::BLINK_OPENING) {
        // Restore lid positions if we were mid-blink
        switch (_blinkEye) {
            case BlinkEye::BOTH:
                setLids(_blinkPrevLeft, _blinkPrevRight);
                break;
            case BlinkEye::LEFT:
                setLeftLid(_blinkPrevLeft);
                break;
            case BlinkEye::RIGHT:
                setRightLid(_blinkPrevRight);
                break;
        }
    }
    _animState = AnimState::IDLE;
}

// === Parameters ===

void EyeController::setCoupling(float c) {
    _coupling = constrain(c, -1.0f, 1.0f);
    applyGaze();  // Reapply with new coupling
}

void EyeController::setMaxVergence(float v) {
    _maxVergence = constrain(v, 0.0f, 100.0f);
    applyGaze();  // Reapply with new vergence
}

void EyeController::center() {
    _gazeX = 0;
    _gazeY = 0;
    // Note: Z and coupling are intentionally not reset - user controls them independently
    _lidLeft = 0;   // Calibration center (neutral open)
    _lidRight = 0;  // Calibration center (neutral open)
    applyGaze();
    applyLids();
}

void EyeController::resetAll() {
    // Full reset including Z and coupling - used for mode switching
    // Cancel any ongoing animation first
    _animState = AnimState::IDLE;

    _gazeX = 0;
    _gazeY = 0;
    _gazeZ = 0;
    _coupling = 1.0f;
    _lidLeft = 0;
    _lidRight = 0;
    applyGaze();
    applyLids();
}

void EyeController::reapply() {
    // Re-apply current internal state to servos
    // Used when returning to Control tab from Calibration
    applyGaze();
    applyLids();
}

// === Internal: Vergence Calculation ===

float EyeController::calculateVergence(float z) {
    // z: -100 (very close) to +100 (far)
    // Returns vergence offset to add/subtract from X
    //
    // At z=+100 (far): vergence = 0 (parallel eyes)
    // At z=0 (medium): vergence = maxVergence / 2
    // At z=-100 (close/nose): vergence = maxVergence (cross-eyed)

    // Normalize Z from [-100, +100] to [0, 1] where 0=far, 1=close
    float normalizedZ = (100.0f - z) / 200.0f;  // z=100 -> 0, z=-100 -> 1

    return _maxVergence * normalizedZ;
}

// === Internal: Logical to Servo Mapping ===

void EyeController::setServoFromLogical(uint8_t servoIndex, float logical) {
    // logical: -100 to +100
    // Maps to servo's calibrated range (min/center/max)

    const ServoConfig& config = servoController.getConfig(servoIndex);

    float position;
    if (logical < 0) {
        // Map -100..0 to min..center
        position = mapFloat(logical, -100.0f, 0.0f, (float)config.min, (float)config.center);
    } else {
        // Map 0..+100 to center..max
        position = mapFloat(logical, 0.0f, 100.0f, (float)config.center, (float)config.max);
    }

    servoController.setPosition(servoIndex, (uint8_t)constrain(position, 0.0f, 180.0f));
}

// === Internal: Apply Gaze to Servos ===

void EyeController::applyGaze() {
    // Calculate vergence offset based on Z (depth)
    float vergenceOffset = calculateVergence(_gazeZ);

    // Apply coupling to vergence
    // coupling > 0: eyes converge (normal)
    // coupling = 0: eyes move independently (no vergence effect)
    // coupling < 0: eyes diverge (wall-eyed)
    float leftXOffset = vergenceOffset * _coupling;
    float rightXOffset = -vergenceOffset * _coupling;

    // Calculate per-eye X positions
    float leftEyeX = constrain(_gazeX + leftXOffset, -100.0f, 100.0f);
    float rightEyeX = constrain(_gazeX + rightXOffset, -100.0f, 100.0f);

    // Y: apply vertical divergence when coupling is negative (Feldman mode)
    // Fixed offset that doesn't depend on gaze, scales with negative coupling
    float verticalDivergence = (_coupling < 0) ? _maxVerticalDivergence * (-_coupling) : 0;
    float leftEyeY = constrain(_gazeY + verticalDivergence, -100.0f, 100.0f);
    float rightEyeY = constrain(_gazeY - verticalDivergence, -100.0f, 100.0f);

    // Apply to servos
    setServoFromLogical(SERVO_LEFT_EYE_X, leftEyeX);
    setServoFromLogical(SERVO_LEFT_EYE_Y, leftEyeY);
    setServoFromLogical(SERVO_RIGHT_EYE_X, rightEyeX);
    setServoFromLogical(SERVO_RIGHT_EYE_Y, rightEyeY);
}

// === Internal: Apply Lids to Servos ===

void EyeController::applyLids() {
    setServoFromLogical(SERVO_LEFT_EYELID, _lidLeft);
    setServoFromLogical(SERVO_RIGHT_EYELID, _lidRight);
}
