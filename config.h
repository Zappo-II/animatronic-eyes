/*
 * Animatronic Eyes
 * Copyright (c) 2025 Zappo-II
 * Licensed under CC BY-NC-SA 4.0
 * https://github.com/Zappo-II/animatronic-eyes
 */

#ifndef CONFIG_H
#define CONFIG_H

// Firmware version
#define FIRMWARE_VERSION "1.1.0"
#define MIN_UI_VERSION "1.1.0"  // Minimum UI version required by this firmware

// Serial logging
#define SERIAL_BAUD 115200

// WiFi AP defaults
#define DEFAULT_AP_SSID_PREFIX "LookIntoMyEyes"
#define DEFAULT_AP_PASSWORD "12345678"
#define AP_CHANNEL 1

// WiFi STA settings
#define WIFI_CONNECT_TIMEOUT_MS 30000
#define WIFI_MAX_NETWORKS 2

// WiFi reconnection defaults (configurable via Web UI)
#define DEFAULT_WIFI_GRACE_MS 3000          // Ignore brief disconnects (1-10s)
#define DEFAULT_WIFI_RETRIES 3              // Retries per network (1-10)
#define DEFAULT_WIFI_RETRY_DELAY_MS 10000   // Delay between retries (5-60s)
#define DEFAULT_WIFI_AP_SCAN_MS 300000      // Check for network while in AP (1-30 min)
#define DEFAULT_WIFI_KEEP_AP true           // Keep AP active when STA connected

// mDNS defaults
#define DEFAULT_MDNS_ENABLED true
#define DEFAULT_MDNS_HOSTNAME "animatronic-eyes"

// LED status defaults
#define DEFAULT_LED_ENABLED true
#define DEFAULT_LED_PIN 2
#define DEFAULT_LED_BRIGHTNESS 255  // 1-255, PWM duty cycle

// Mode System defaults
#define DEFAULT_MODE "follow"           // Default startup mode
#define DEFAULT_AUTO_BLINK true         // Enable automatic blinking
#define DEFAULT_BLINK_INTERVAL_MIN 2000 // Minimum ms between auto-blinks
#define DEFAULT_BLINK_INTERVAL_MAX 6000 // Maximum ms between auto-blinks
#define DEFAULT_MIRROR_PREVIEW false    // Mirror eye preview (flip horizontal)

// Impulse System defaults
#define DEFAULT_AUTO_IMPULSE true              // Enable automatic impulses
#define DEFAULT_IMPULSE_INTERVAL_MIN 15000     // 15 seconds minimum
#define DEFAULT_IMPULSE_INTERVAL_MAX 25000     // 25 seconds maximum
#define DEFAULT_IMPULSE_SELECTION "startle,distraction"  // Default selected impulses

// Web server
#define HTTP_PORT 80
#define WEBSOCKET_PATH "/ws"

// LittleFS partition size (must match partition scheme)
// "Default 4MB with spiffs" = 0x160000 (1441792 bytes)
#define LITTLEFS_PARTITION_SIZE 0x160000

// Servo count
#define NUM_SERVOS 6

// Servo indices
#define SERVO_LEFT_EYE_X    0
#define SERVO_LEFT_EYE_Y    1
#define SERVO_LEFT_EYELID   2
#define SERVO_RIGHT_EYE_X   3
#define SERVO_RIGHT_EYE_Y   4
#define SERVO_RIGHT_EYELID  5

// Default servo pins
#define DEFAULT_PIN_LEFT_EYE_X    32
#define DEFAULT_PIN_LEFT_EYE_Y    33
#define DEFAULT_PIN_LEFT_EYELID   25
#define DEFAULT_PIN_RIGHT_EYE_X   26
#define DEFAULT_PIN_RIGHT_EYE_Y   27
#define DEFAULT_PIN_RIGHT_EYELID  14

// Default servo calibration
#define DEFAULT_SERVO_MIN     89
#define DEFAULT_SERVO_CENTER  90
#define DEFAULT_SERVO_MAX     91

// Servo movement
#define SERVO_SPEED_DEFAULT 100  // degrees per second
#define SERVO_UPDATE_INTERVAL_MS 20  // Minimum ms between servo writes (prevents watchdog)

// WebSocket broadcast interval
#define WS_BROADCAST_INTERVAL_MS 100

// NVS namespace
#define NVS_NAMESPACE "animeyes"

// Admin Lock
#define ADMIN_TIMEOUT_MS 900000         // 15 minutes unlock timeout
#define ADMIN_MAX_FAILED_ATTEMPTS 3     // Failed attempts before lockout
#define ADMIN_LOCKOUT_MS 300000         // 5 minute lockout after max failures

// Update Check
#define UPDATE_CHECK_BOOT_DELAY_MS  30000     // 30s delay after boot before first check
#define UPDATE_CHECK_JITTER_MS      1800000   // 30 min max random jitter
#define GITHUB_VERSION_URL "https://raw.githubusercontent.com/Zappo-II/animatronic-eyes/main/data/version.json"
#define GITHUB_RELEASES_URL "https://github.com/Zappo-II/animatronic-eyes/releases"

// Update check interval options (in ms)
#define UPDATE_INTERVAL_BOOT_ONLY   0
#define UPDATE_INTERVAL_DAILY       86400000   // 24 hours
#define UPDATE_INTERVAL_WEEKLY      604800000  // 7 days

// Update check defaults
#define DEFAULT_UPDATE_CHECK_ENABLED true
#define DEFAULT_UPDATE_CHECK_INTERVAL 1  // 0=boot only, 1=daily, 2=weekly

#endif // CONFIG_H
