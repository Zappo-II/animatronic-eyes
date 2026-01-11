# Animatronic Eyes

[![Release](https://img.shields.io/github/v/release/Zappo-II/animatronic-eyes)](https://github.com/Zappo-II/animatronic-eyes/releases)
[![License](https://img.shields.io/badge/license-CC%20BY--NC--SA%204.0-blue)](LICENSE)
[![Downloads](https://img.shields.io/github/downloads/Zappo-II/animatronic-eyes/total)](https://github.com/Zappo-II/animatronic-eyes/releases)

ESP32-based controller for 3D printed animatronic eyes with a web interface.

Control realistic eye movements, eyelid animations, and gaze tracking through your browser - no app required.

## Features

- **Intuitive Web UI** - Control eyes from any device with a browser
- **Mode System** - Autonomous behaviors (Natural, Sleepy, Alert, Spy, Crazy) - [create your own!](docs/usage.md#custom-modes)
- **Impulse System** - Triggered reactions (Startle, Distraction) - [fully customizable](docs/usage.md#custom-impulses)
- **Follow Mode** - Manual control with touch-friendly gaze pad
- **Gaze Control** - Natural eye movement with vergence (eyes converge on close objects)
- **Eyelid Control** - Independent or linked eyelid movement with blink animations
- **Auto-Blink** - Configurable automatic blinking for realistic behavior
- **Live Calibration** - Tune servo positions in real-time
- **Admin Lock** - PIN protection for configuration settings
- **WiFi Connectivity** - Connect to your home network or use the built-in access point
- **OTA Updates** - Update firmware and UI wirelessly
- **Recovery Mode** - Built-in recovery UI if something goes wrong

<img src="docs/AnimatronicEyes.png" width="600">

## Hardware Requirements

| Component | Specification | Notes |
|-----------|--------------|-------|
| Microcontroller | [ESP32 Dev Kit C V4](https://www.az-delivery.de/products/esp-32-dev-kit-c-v4) | Any ESP32 with sufficient GPIO |
| Servos | 6x [MG90S](https://www.az-delivery.de/products/mg90s-micro-servomotor) | Metal gear micro servos |
| 3D Printed Parts | [Morgan Manly's designs](https://www.instructables.com/member/MorganManly/) | See [Acknowledgements](#acknowledgements) <br> Other device hardware might also work... |
| Power Supply | 5V 2A+ | Servos draw significant current |

## Quick Start

1. **Download** `animatronic-eyes.ino.bin` and `ui.bin` from [Releases](https://github.com/Zappo-II/animatronic-eyes/releases)
2. **Flash firmware via USB** using one of these options:
   - **Web Flasher** (easiest): Use [ESP Web Tools](https://web.esphome.io/) or [Espressif Launchpad](https://espressif.github.io/esp-launchpad/) - no install required
   - **Arduino IDE**: Open any sketch, select your board/port, then Sketch → Upload Compiled Binary
   - **Command line**: `esptool.py --chip esp32 --port /dev/ttyUSB0 write_flash 0x10000 animatronic-eyes.ino.bin`
     - Find esptool: `pip install esptool` or check `~/.arduino15/packages/esp32/tools/esptool_py/*/`
   - **Makefile**: Uses `docker` - see the [Setup Guide](docs/setup.md)
3. **Connect** to WiFi `LookIntoMyEyes-XXXXXX` (password: `12345678`)
4. **Open** http://192.168.4.1
5. **Upload UI** via the recovery page of the device's firmware

Subsequent updates (firmware and UI) can be uploaded via the web interface - no USB required.

> **Power Warning:** Never connect USB 5V and external 5V to the ESP32 at the same time - only share ground. See [Power Configuration Options](docs/troubleshooting.md#power-configuration-options) for safe wiring.

To build from source, see the [Setup Guide](docs/setup.md).

## Documentation

| Document | Description |
|----------|-------------|
| [Setup Guide](docs/setup.md) | Complete installation instructions |
| [Usage Guide](docs/usage.md) | How to use the web interface |
| [Calibration Guide](docs/calibration.md) | Servo calibration process |
| [Troubleshooting](docs/troubleshooting.md) | Common problems and solutions |
| [Architecture](docs/architecture.md) | Technical overview for developers |
| [Development Guide](docs/development.md) | Contributing and modifying the code |
| [Roadmap](docs/roadmap.md) | Detailed specs for planned features |
| [Changelog](CHANGELOG.md) | Version history |

## Current Version

**v1.1.0** - Docker build system, Mirror Preview toggle, OTA deployment via Makefile.

## Roadmap

| Version | Feature | Status |
|---------|---------|--------|
| v0.2 | **Core** - Servo control, WiFi, WebSocket, OTA | ✅ Complete |
| v0.3 | **Connectivity** - Multi-SSID, mDNS, LED status | ✅ Complete |
| v0.4 | **UI Polish** - Tabs, console, recovery mode | ✅ Complete |
| v0.5 | **Eye Controller** - Gaze abstraction, vergence | ✅ Complete |
| v0.6 | **Follow Mode** - Touch gaze pad control | ✅ Complete |
| v0.7 | **Mode System** - Autonomous behaviors | ✅ Complete |
| v0.8 | **Impulses** - Triggered reactions (Startle, Distraction) | ✅ Complete |
| v0.9 | **Admin Lock** - PIN protection for settings | ✅ Complete |
| v1.0 | **Update Check** - GitHub version notifications | ✅ Complete |

## License

This project is licensed under [CC BY-NC-SA 4.0](https://creativecommons.org/licenses/by-nc-sa/4.0/).

**You are free to:**
- Share and adapt for non-commercial purposes
- Learn, tinker, and build your own

**Commercial use is strictly prohibited without explicit permission.**

See [LICENSE](LICENSE) for details.

## Acknowledgements

Device - Hardware designed by **Morgan Manly**:
- [GitHub](https://github.com/ManlyMorgan)
- [Instructables](https://www.instructables.com/member/MorganManly/)
- [Makerworld](https://makerworld.com/en/@manlymorgan)

## Third-Party Libraries

| Library | Author | License |
|---------|--------|---------|
| [ESPAsyncWebServer](https://github.com/ESP32Async/ESPAsyncWebServer) | ESP32Async | LGPL-3.0 |
| [AsyncTCP](https://github.com/ESP32Async/AsyncTCP) | ESP32Async | LGPL-3.0 |
| [ArduinoJson](https://github.com/bblanchon/ArduinoJson) | Benoit Blanchon | MIT |
| [ESP32Servo](https://github.com/madhephaestus/ESP32Servo) | Kevin Harrington | LGPL-2.1+ |

## Contributing

Contributions are welcome! Please:
1. Check existing issues before creating new ones
2. Follow the code conventions in [Development Guide](docs/development.md)
3. Test your changes thoroughly
4. Update documentation as needed

## Support

Having issues? Check the [Troubleshooting Guide](docs/troubleshooting.md) first.

Still stuck? [Open an issue](https://github.com/Zappo-II/animatronic-eyes/issues) with:
- Firmware version
- Steps to reproduce
- Serial Monitor output
