/*
 * Animatronic Eyes
 * Copyright (c) 2025 Zappo-II
 * Licensed under CC BY-NC-SA 4.0
 * https://github.com/Zappo-II/animatronic-eyes
 *
 * ESP32-based servo controller for 3D printed animatronic eyes
 */

// Libraries must be included in main .ino for Arduino IDE linking
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <ESP32Servo.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <Update.h>
#include <ESPmDNS.h>

#include "config.h"
#include "storage.h"
#include "led_status.h"
#include "wifi_manager.h"
#include "servo_controller.h"
#include "eye_controller.h"
#include "auto_blink.h"
#include "mode_manager.h"
#include "mode_player.h"
#include "impulse_player.h"
#include "auto_impulse.h"
#include "update_checker.h"
#include "web_server.h"

void setup() {
    Serial.begin(SERIAL_BAUD);
    delay(1000);

    Serial.println();
    Serial.println("================================");
    Serial.printf("Animatronic Eyes v%s\n", FIRMWARE_VERSION);
    Serial.println("================================");
    Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());

    // Initialize components
    // IMPORTANT: Initialization order matters due to ESP32 LEDC timer allocation.
    // Servos MUST init before LED - both use LEDC for PWM, and servo library needs
    // to claim timers first (50Hz). If LED inits first, it takes a timer and servo 0
    // fails to attach. This caused Left Eye X to stop responding (fixed in v0.4.10).
    storage.begin();
    servoController.begin();  // Servos first - claims LEDC timers for 50Hz PWM
    eyeController.begin();    // Eye Controller - abstraction layer over servos
    ledStatus.begin();        // LED second - uses remaining LEDC resources
    wifiManager.begin();      // WiFi also starts mDNS if connected

    // Mount LittleFS early - mode system needs it for mode files
    if (!LittleFS.begin(true)) {
        Serial.println("[LittleFS] Mount failed!");
    }

    // Mode system - must init after eye controller, storage, and LittleFS
    autoBlink.begin();        // Auto-blink background system
    modeManager.begin();      // Mode manager - handles mode state and transitions
    // Note: modePlayer has no begin() - it initializes on first loadMode()

    // Impulse system - must init after storage and LittleFS
    impulsePlayer.begin();    // Impulse player - preloads first impulse
    autoImpulse.begin();      // Auto-impulse background system

    webServer.begin();

    // Update checker - must init after WiFi and storage
    updateChecker.begin();

    Serial.println();
    Serial.println("Setup complete!");
    Serial.printf("Free heap after init: %d bytes\n", ESP.getFreeHeap());

    // Log startup to web console (will be in buffer for first client)
    WEB_LOG("System", "Animatronic Eyes v%s ready", FIRMWARE_VERSION);
    WEB_LOG("System", "Free heap: %d bytes", ESP.getFreeHeap());
}

void loop() {
    ledStatus.loop();
    wifiManager.loop();
    servoController.loop();
    eyeController.loop();
    autoBlink.loop();
    modePlayer.loop();
    impulsePlayer.loop();
    autoImpulse.loop();
    updateChecker.loop();
    webServer.loop();
}
