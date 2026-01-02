/*
 * Animatronic Eyes
 * Copyright (c) 2025 Zappo-II
 * Licensed under CC BY-NC-SA 4.0
 * https://github.com/Zappo-II/animatronic-eyes
 */

#ifndef LED_STATUS_H
#define LED_STATUS_H

#include <Arduino.h>

enum LedPattern {
    LED_PATTERN_OFF,
    LED_PATTERN_SOLID,
    LED_PATTERN_SLOW_BLINK,      // AP mode, no STA configured (1s on, 1s off)
    LED_PATTERN_DOUBLE_BLINK,    // AP mode, fallback from STA
    LED_PATTERN_FAST_BLINK,      // Connecting/reconnecting (200ms)
    LED_PATTERN_VERY_FAST_BLINK, // OTA in progress (100ms)
    LED_PATTERN_STROBE           // Factory reset (50ms)
};

class LedStatus {
public:
    void begin();
    void loop();

    void setPattern(LedPattern pattern);
    LedPattern getPattern();

    void setEnabled(bool enabled);
    bool isEnabled();

    void setPin(uint8_t pin);
    uint8_t getPin();

    void setBrightness(uint8_t brightness);
    uint8_t getBrightness();

    // Convenience methods
    void off();
    void solid();
    void slowBlink();
    void doubleBlink();
    void fastBlink();
    void veryFastBlink();
    void strobe();

private:
    uint8_t _pin = 2;
    bool _enabled = true;
    uint8_t _brightness = 255;
    LedPattern _pattern = LED_PATTERN_OFF;
    bool _ledState = false;
    unsigned long _lastToggle = 0;
    uint8_t _blinkPhase = 0;  // For double blink pattern

    void updateLed(bool state);
    void handlePattern();
};

extern LedStatus ledStatus;

#endif // LED_STATUS_H
