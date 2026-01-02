/*
 * Animatronic Eyes
 * Copyright (c) 2025 Zappo-II
 * Licensed under CC BY-NC-SA 4.0
 * https://github.com/Zappo-II/animatronic-eyes
 */

#include "led_status.h"
#include "storage.h"
#include "config.h"
#include "web_server.h"

// LEDC PWM configuration
#define LED_PWM_FREQ 5000    // 5 kHz PWM frequency
#define LED_PWM_RESOLUTION 8 // 8-bit resolution (0-255)

// ESP32 input-only pins (cannot be used for LED output)
static bool isInputOnlyPin(uint8_t pin) {
    return (pin >= 34 && pin <= 39);
}

LedStatus ledStatus;

void LedStatus::begin() {
    LedConfig config = storage.getLedConfig();
    _enabled = config.enabled;
    _pin = config.pin;
    _brightness = config.brightness;

    if (isInputOnlyPin(_pin)) {
        WEB_LOG("LED", "WARNING: Pin %d is input-only, LED will not work", _pin);
    }

    // Setup LEDC PWM - new API: ledcAttach(pin, freq, resolution) returns channel or 0 on failure
    ledcAttach(_pin, LED_PWM_FREQ, LED_PWM_RESOLUTION);
    ledcWrite(_pin, 0);  // Start with LED off
    _ledState = false;
    _lastToggle = millis();
}

void LedStatus::loop() {
    if (!_enabled) {
        if (_ledState) {
            updateLed(false);
        }
        return;
    }

    handlePattern();
}

void LedStatus::handlePattern() {
    unsigned long now = millis();
    unsigned long elapsed = now - _lastToggle;

    switch (_pattern) {
        case LED_PATTERN_OFF:
            if (_ledState) updateLed(false);
            break;

        case LED_PATTERN_SOLID:
            if (!_ledState) updateLed(true);
            break;

        case LED_PATTERN_SLOW_BLINK:
            // 1s on, 1s off
            if (elapsed >= 1000) {
                updateLed(!_ledState);
                _lastToggle = now;
            }
            break;

        case LED_PATTERN_DOUBLE_BLINK:
            // Two quick flashes (200ms each), then 1s pause
            // Phase 0: LED on (200ms)
            // Phase 1: LED off (200ms)
            // Phase 2: LED on (200ms)
            // Phase 3: LED off (1000ms)
            switch (_blinkPhase) {
                case 0:
                    if (!_ledState) updateLed(true);
                    if (elapsed >= 200) {
                        _blinkPhase = 1;
                        _lastToggle = now;
                    }
                    break;
                case 1:
                    if (_ledState) updateLed(false);
                    if (elapsed >= 200) {
                        _blinkPhase = 2;
                        _lastToggle = now;
                    }
                    break;
                case 2:
                    if (!_ledState) updateLed(true);
                    if (elapsed >= 200) {
                        _blinkPhase = 3;
                        _lastToggle = now;
                    }
                    break;
                case 3:
                    if (_ledState) updateLed(false);
                    if (elapsed >= 1000) {
                        _blinkPhase = 0;
                        _lastToggle = now;
                    }
                    break;
            }
            break;

        case LED_PATTERN_FAST_BLINK:
            // 200ms on, 200ms off
            if (elapsed >= 200) {
                updateLed(!_ledState);
                _lastToggle = now;
            }
            break;

        case LED_PATTERN_VERY_FAST_BLINK:
            // 100ms on, 100ms off
            if (elapsed >= 100) {
                updateLed(!_ledState);
                _lastToggle = now;
            }
            break;

        case LED_PATTERN_STROBE:
            // 50ms on, 50ms off
            if (elapsed >= 50) {
                updateLed(!_ledState);
                _lastToggle = now;
            }
            break;
    }
}

void LedStatus::updateLed(bool state) {
    _ledState = state;
    ledcWrite(_pin, state ? _brightness : 0);
}

void LedStatus::setPattern(LedPattern pattern) {
    if (_pattern != pattern) {
        _pattern = pattern;
        _blinkPhase = 0;
        _lastToggle = millis();
    }
}

LedPattern LedStatus::getPattern() {
    return _pattern;
}

void LedStatus::setEnabled(bool enabled) {
    _enabled = enabled;
    if (!enabled) {
        updateLed(false);
    }

    // Save to storage
    LedConfig config;
    config.enabled = enabled;
    config.pin = _pin;
    config.brightness = _brightness;
    storage.setLedConfig(config);
}

bool LedStatus::isEnabled() {
    return _enabled;
}

void LedStatus::setPin(uint8_t pin) {
    if (pin != _pin) {
        if (isInputOnlyPin(pin)) {
            WEB_LOG("LED", "WARNING: Pin %d is input-only, LED will not work", pin);
        }

        // Detach LEDC from old pin
        ledcDetach(_pin);

        // Setup new pin with LEDC
        _pin = pin;
        ledcAttach(_pin, LED_PWM_FREQ, LED_PWM_RESOLUTION);
        ledcWrite(_pin, _ledState ? _brightness : 0);

        // Save to storage
        LedConfig config;
        config.enabled = _enabled;
        config.pin = pin;
        config.brightness = _brightness;
        storage.setLedConfig(config);
    }
}

uint8_t LedStatus::getPin() {
    return _pin;
}

void LedStatus::setBrightness(uint8_t brightness) {
    // Clamp to valid range (1-255)
    if (brightness < 1) brightness = 1;

    if (brightness != _brightness) {
        _brightness = brightness;

        // Update LED if currently on
        if (_ledState) {
            ledcWrite(_pin, _brightness);
        }

        // Save to storage
        LedConfig config;
        config.enabled = _enabled;
        config.pin = _pin;
        config.brightness = _brightness;
        storage.setLedConfig(config);
    }
}

uint8_t LedStatus::getBrightness() {
    return _brightness;
}

// Convenience methods
void LedStatus::off() {
    setPattern(LED_PATTERN_OFF);
}

void LedStatus::solid() {
    setPattern(LED_PATTERN_SOLID);
}

void LedStatus::slowBlink() {
    setPattern(LED_PATTERN_SLOW_BLINK);
}

void LedStatus::doubleBlink() {
    setPattern(LED_PATTERN_DOUBLE_BLINK);
}

void LedStatus::fastBlink() {
    setPattern(LED_PATTERN_FAST_BLINK);
}

void LedStatus::veryFastBlink() {
    setPattern(LED_PATTERN_VERY_FAST_BLINK);
}

void LedStatus::strobe() {
    setPattern(LED_PATTERN_STROBE);
}
