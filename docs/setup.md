# Setup Guide

Complete installation instructions for the Animatronic Eyes project.

## Prerequisites

Choose your build method:

- **Arduino IDE** - GUI-based, good for beginners and one-off builds
- **Command-line (Docker)** - Reproducible builds, automation, no local tool installation

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

# Option A: Arduino IDE

## Step 1: Install Arduino IDE

1. Download Arduino IDE from [arduino.cc](https://www.arduino.cc/en/software)
2. Install and launch (version 2.x recommended)

## Step 2: Add ESP32 Board Support

1. Open **File → Preferences**
2. In "Additional Board Manager URLs", add:
   ```
   https://dl.espressif.com/dl/package_esp32_index.json
   ```
3. Click **OK**
4. Open **Tools → Board → Boards Manager**
5. Search for "esp32"
6. Install **"ESP32 by Espressif Systems"** version **3.3.5**

## Step 3: Install Required Libraries

Open **Sketch → Include Library → Manage Libraries** and install:

| Library | Author | Version | Notes |
|---------|--------|---------|-------|
| **ESPAsyncWebServer** | ESP32Async | 3.9.4 | Async web server with WebSocket |
| **AsyncTCP** | ESP32Async | 3.4.10 | Required by ESPAsyncWebServer |
| **ArduinoJson** | Benoit Blanchon | 7.4.2 | JSON parsing |
| **ESP32Servo** | Kevin Harrington | 3.0.9 | PWM servo control |

### Important: Use the Correct Libraries

The original me-no-dev libraries are archived and no longer maintained. Use the **ESP32Async** forks:

- ESPAsyncWebServer: https://github.com/ESP32Async/ESPAsyncWebServer
- AsyncTCP: https://github.com/ESP32Async/AsyncTCP

In Arduino Library Manager, search for "ESPAsyncWebServer" and select the one by **"ESP32Async"** (not "me-no-dev" or "mathieucarbou").

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
Sketch uses 1303785 bytes (99%) of program storage space.
Global variables use 53848 bytes (16%) of dynamic memory.
```

Note: 99% flash usage is normal - the firmware is optimized to fit.

## Step 7: Upload Web UI Files

1. With the sketch open, press **Ctrl+Shift+P** (Cmd+Shift+P on Mac)
2. Type "Upload LittleFS" and select it
3. Wait for upload to complete

Or from menu: **Tools → ESP32 Sketch Data Upload**

---

# Option B: Command-Line (Docker)

Reproducible builds using Docker - no need to install Arduino IDE, libraries, or tools locally.

## Prerequisites

Install these system packages:

| Package | Purpose |
|---------|---------|
| docker | Build environment |
| make | Build automation |
| picocom | Serial monitor |
| avahi | Device discovery (mDNS) |
| curl | OTA deployment |
| gh | GitHub releases (optional) |

**Arch/Manjaro:**
```bash
pacman -S docker picocom github-cli avahi
sudo systemctl enable --now docker
sudo usermod -aG docker $USER  # Log out and back in
```

**Ubuntu/Debian:**
```bash
apt install docker.io picocom gh avahi-utils curl
sudo usermod -aG docker $USER  # Log out and back in
```

## Step 1: Build Docker Image (One-Time)

```bash
make docker
```

This creates a Docker image with:
- arduino-cli with ESP32 core 3.3.5
- All required libraries (exact versions)
- mklittlefs for UI filesystem
- esptool for flashing

## Step 2: Compile Firmware and UI

```bash
make build
```

Output files in `build/`:
- `animatronic-eyes.ino.bin` - Firmware
- `ui.bin` - Web UI filesystem

## Step 3: Flash via USB

```bash
make flash-all          # Flash firmware and UI
```

Or separately:
```bash
make flash              # Firmware only
make flash-ui           # UI only
```

Default port is `/dev/ttyUSB0`. Override with:
```bash
make flash-all PORT=/dev/ttyACM0
```

## Step 4: Serial Monitor

```bash
make monitor
```

Exit with `Ctrl+A` then `Ctrl+X`.

## OTA Deployment (Wireless)

After initial USB flash, deploy updates wirelessly:

```bash
make discover           # Find device on network, save IP
make deploy-firmware    # Upload firmware (device reboots)
make deploy-ui PIN=1234 # Upload UI with admin PIN
```

## All Make Targets

| Target | Description |
|--------|-------------|
| `make help` | Show all targets and options |
| `make docker` | Build Docker image (one-time) |
| `make build` | Compile firmware and ui.bin |
| `make flash` | Flash firmware via USB |
| `make flash-ui` | Flash UI via USB |
| `make flash-all` | Flash both via USB |
| `make discover` | Find devices on network (mDNS) |
| `make deploy-firmware` | Upload firmware via OTA |
| `make deploy-ui` | Upload UI via OTA |
| `make monitor` | Open serial monitor |
| `make clean` | Remove build artifacts |
| `make release V=x.y.z` | Create GitHub release |

## Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `PORT` | /dev/ttyUSB0 | Serial port for USB flash |
| `BAUD` | 115200 | Baud rate for monitor |
| `DEVICE` | (from .device) | Device IP for OTA |
| `PIN` | - | Admin PIN for OTA |
| `V` | - | Version for release |

---

# First Boot

1. Open Serial Monitor:
   - Arduino IDE: **Tools → Serial Monitor** (115200 baud)
   - Command-line: `make monitor`
2. Press the ESP32 reset button

### Expected Serial Output

```
================================
Animatronic Eyes v1.1.0
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

## Connect to Device

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
- **Web page doesn't load**: Ensure UI was uploaded (Step 7 / `make flash-ui`)
- **OTA deploy fails**: Check device is on same network, try `make discover`
