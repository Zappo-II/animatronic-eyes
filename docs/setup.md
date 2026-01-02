# Setup Guide

Complete installation instructions for the Animatronic Eyes project.

## Prerequisites

- Arduino IDE 2.x (or 1.8.x)
- USB cable for ESP32 programming
- Assembled animatronic eyes hardware (see [Hardware Requirements](#hardware-requirements))

## Hardware Requirements

| Component | Specification | Notes |
|-----------|--------------|-------|
| Microcontroller | [ESP32 Dev Kit C V4](https://www.az-delivery.de/products/esp-32-dev-kit-c-v4) | Any ESP32 with sufficient GPIO should work |
| Servos | 6x [MG90S](https://www.az-delivery.de/products/mg90s-micro-servomotor) | Metal gear micro servos |
| 3D Printed Parts | Morgan Manly's designs | See [Acknowledgements](../README.md#acknowledgements) |
| Power Supply | 5V 2A+ | Servos draw significant current |

### Default GPIO Pin Assignments

| Servo | Function | Default GPIO |
|-------|----------|--------------|
| 0 | Left Eye X | 32 |
| 1 | Left Eye Y | 33 |
| 2 | Left Eyelid | 25 |
| 3 | Right Eye X | 26 |
| 4 | Right Eye Y | 27 |
| 5 | Right Eyelid | 14 |
| - | Status LED | 2 |

All pins are configurable via the web UI.

---

## Step 1: Install Arduino IDE

1. Download Arduino IDE from [arduino.cc](https://www.arduino.cc/en/software)
2. Install and launch

## Step 2: Add ESP32 Board Support

1. Open **File → Preferences**
2. In "Additional Board Manager URLs", add:
   ```
   https://dl.espressif.com/dl/package_esp32_index.json
   ```
3. Click **OK**
4. Open **Tools → Board → Boards Manager**
5. Search for "esp32"
6. Install **"ESP32 by Espressif Systems"** (version 3.x recommended)

## Step 3: Install Required Libraries

Open **Sketch → Include Library → Manage Libraries** and install:

| Library | Author | Version | Notes |
|---------|--------|---------|-------|
| **ESPAsyncWebServer** | ESP32Async | 3.9.x | Async web server with WebSocket |
| **AsyncTCP** | ESP32Async | 3.4.x | Required by ESPAsyncWebServer |
| **ArduinoJson** | Benoit Blanchon | 7.4.x | JSON parsing |
| **ESP32Servo** | Kevin Harrington | 3.0.x | PWM servo control |

### Important: Use Maintained Library Forks

The original me-no-dev libraries are archived and no longer maintained. Use the ESP32Async forks:

- ESPAsyncWebServer: https://github.com/ESP32Async/ESPAsyncWebServer
- AsyncTCP: https://github.com/ESP32Async/AsyncTCP

These are available in the Arduino Library Manager. Search for "ESPAsyncWebServer" and select the one by "ESP32Async".

## Step 4: Install LittleFS Upload Plugin

The web UI files must be uploaded to the ESP32's filesystem separately from the firmware.

### For Arduino IDE 2.x

1. Download `arduino-littlefs-upload-*.vsix` from:
   https://github.com/earlephilhower/arduino-littlefs-upload/releases
2. Create the plugins folder if it doesn't exist:
   ```bash
   mkdir -p ~/.arduinoIDE/plugins/
   ```
3. Copy the `.vsix` file to `~/.arduinoIDE/plugins/`
4. Restart Arduino IDE

### For Arduino IDE 1.8.x

1. Download the ESP32 LittleFS plugin from:
   https://github.com/lorol/arduino-esp32littlefs-plugin/releases
2. Create `tools` folder in your Arduino sketchbook directory
3. Extract the plugin there
4. Restart Arduino IDE

## Step 5: Configure Board Settings

1. **Tools → Board → ESP32 Arduino → ESP32 Dev Module**
2. Configure these settings:

| Setting | Value |
|---------|-------|
| Upload Speed | 921600 |
| CPU Frequency | 240MHz |
| Flash Frequency | 80MHz |
| Flash Mode | QIO |
| Flash Size | 4MB (32Mb) |
| Partition Scheme | Default 4MB with spiffs (1.2MB APP/1.5MB SPIFFS) |
| PSRAM | Disabled |

## Step 6: Compile and Upload Firmware

1. Open `animatronic-eyes.ino`
2. Connect ESP32 via USB
3. Select the correct port: **Tools → Port → /dev/ttyUSB0** (or your port)
4. Click **Verify** (checkmark) to compile
5. Click **Upload** (arrow) to flash firmware

### Expected Compile Output

```
Sketch uses 1088391 bytes (83%) of program storage space.
Global variables use 45312 bytes (13%) of dynamic memory.
```

## Step 7: Upload Web UI Files

1. With the sketch open, press **Ctrl+Shift+P** (Cmd+Shift+P on Mac)
2. Type "Upload LittleFS" and select it
3. Wait for upload to complete

Or from menu: **Tools → ESP32 Sketch Data Upload**

---

## First Boot

1. Open **Serial Monitor** (Tools → Serial Monitor or Ctrl+Shift+M)
2. Set baud rate to **115200**
3. Press the ESP32 reset button

### Expected Serial Output

```
================================
Animatronic Eyes v0.9.16
================================
Free heap: 289324 bytes
[Storage] Initialized
[Servo] Initializing servos...
[Servo] Left Eye X attached on pin 32, center=90
[Servo] Left Eye Y attached on pin 33, center=90
[Servo] Left Eyelid attached on pin 25, center=90
[Servo] Right Eye X attached on pin 26, center=90
[Servo] Right Eye Y attached on pin 27, center=90
[Servo] Right Eyelid attached on pin 14, center=90
[WiFi] No stored credentials, starting AP mode
[WiFi] AP started: LookIntoMyEyes-XXXXXX (IP: 192.168.4.1)
[WebServer] LittleFS mounted
[WebServer] Started on port 80

Setup complete!
Free heap after init: 212492 bytes
```

## Step 8: Connect to Device

1. On your phone/computer, look for WiFi network: **LookIntoMyEyes-XXXXXX**
   (XXXXXX is your device's unique ID)
2. Connect with password: `12345678`
3. Open browser: http://192.168.4.1

You should see the Animatronic Eyes web interface!

---

## Next Steps

- [Calibrate your servos](calibration.md)
- [Learn the web interface](usage.md)
- [Connect to your home WiFi](usage.md#wifi-configuration)

## Troubleshooting

If something doesn't work, see [Troubleshooting & FAQ](troubleshooting.md).

Common issues:
- **Upload fails**: Check USB cable and port selection
- **No WiFi network appears**: Check serial output for errors
- **Web page doesn't load**: Ensure LittleFS was uploaded (Step 7)
