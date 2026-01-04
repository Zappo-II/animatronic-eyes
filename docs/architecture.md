# Architecture Overview

Technical documentation for developers who want to understand or modify the codebase.

## System Architecture

```
┌─────────────────────────────────────────┐
│           Web UI (Browser)              │
│  index.html / style.css / app.js        │
└─────────────────┬───────────────────────┘
                  │ WebSocket + HTTP
                  ▼
┌─────────────────────────────────────────┐
│           Web Server Module             │
│  HTTP routes, WebSocket, OTA, Recovery  │
└─────────────────┬───────────────────────┘
                  │
    ┌─────────────┼─────────────┐
    ▼             ▼             ▼
┌─────────┐ ┌───────────┐ ┌─────────────┐
│  WiFi   │ │    Eye    │ │   Storage   │
│ Manager │ │ Controller│ │   (NVS)     │
└─────────┘ └─────┬─────┘ └─────────────┘
                  │
                  ▼
          ┌─────────────┐
          │   Servo     │
          │ Controller  │
          └─────────────┘
                  │
                  ▼
          ┌─────────────┐
          │  Hardware   │
          │  (6 Servos) │
          └─────────────┘
```

## Layer Diagram (Eye Controller)

```
┌─────────────────────────────────────┐
│  Impulses (overlay on either)       │  ← v0.8: One-shot animations
│  ImpulsePlayer, AutoImpulse         │     Save/restore state
├─────────────────┬───────────────────┤
│  Follow Mode    │   Mode System     │  ← v0.7: XOR, one active
│  (Manual)       │   (Auto modes)    │
│                 │ ┌───────────────┐ │
│                 │ │ ModeManager   │ │
│                 │ │ ModePlayer    │ │
│                 │ └───────────────┘ │
├─────────────────┴───────────────────┤
│  Auto-Blink (optional layer)        │  ← Works in all modes
├─────────────────────────────────────┤
│  Eye Controller                     │
│  Gaze, Vergence, Coupling, Lids     │
├─────────────────────────────────────┤
│  Calibration Layer                  │
│  min/center/max, invert             │
├─────────────────────────────────────┤
│  Servo Controller                   │
│  ESP32Servo wrapper, paired writes  │
└─────────────────────────────────────┘
```

## Code Flow

### Gaze Command Flow

How a gaze change travels from browser to servo:

```
┌─────────────────────────────────────────────────────────────────────────┐
│ 1. USER: Drags finger on gaze pad                                       │
└─────────────────────────────────────────────────────────────────────────┘
                                    ↓
┌─────────────────────────────────────────────────────────────────────────┐
│ 2. app.js: Calculates X/Y from touch position, throttles to 50ms        │
│    sendCommand({type: 'setGaze', x: 50, y: -30, z: 0})                  │
└─────────────────────────────────────────────────────────────────────────┘
                                    ↓ WebSocket
┌─────────────────────────────────────────────────────────────────────────┐
│ 3. web_server.cpp: handleWebSocketMessage() parses JSON                 │
│    Extracts x, y, z values, calls eyeController.setGaze(x, y, z)        │
└─────────────────────────────────────────────────────────────────────────┘
                                    ↓
┌─────────────────────────────────────────────────────────────────────────┐
│ 4. eye_controller.cpp: setGaze() stores logical coordinates             │
│    - Stores _gazeX, _gazeY, _gazeZ (-100 to +100)                       │
│    - Calls applyGaze() to convert to servo positions                    │
└─────────────────────────────────────────────────────────────────────────┘
                                    ↓
┌─────────────────────────────────────────────────────────────────────────┐
│ 5. eye_controller.cpp: applyGaze()                                      │
│    - Maps logical coords to servo degrees using calibration             │
│    - Calculates vergence offset based on Z depth                        │
│    - Applies coupling factor for eye coordination                       │
│    - Calls servoController.setPosition() for each eye servo             │
└─────────────────────────────────────────────────────────────────────────┘
                                    ↓
┌─────────────────────────────────────────────────────────────────────────┐
│ 6. servo_controller.cpp: setPosition()                                  │
│    - Constrains to calibration min/max                                  │
│    - Sets _targetPositions[index] (queued, not immediate)               │
└─────────────────────────────────────────────────────────────────────────┘
                                    ↓
┌─────────────────────────────────────────────────────────────────────────┐
│ 7. servo_controller.cpp: loop() (called from main loop)                 │
│    - Throttled: max 2 servo writes per 20ms                             │
│    - Prioritizes paired servos (eyelids, eye X, eye Y) for sync         │
│    - Finds servo with pending change (_position != _targetPosition)     │
│    - Applies invert flag if needed                                      │
│    - Calls servos[i].write(position)                                    │
└─────────────────────────────────────────────────────────────────────────┘
                                    ↓
┌─────────────────────────────────────────────────────────────────────────┐
│ 8. HARDWARE: ESP32Servo generates PWM signal, servo moves               │
└─────────────────────────────────────────────────────────────────────────┘
```

### State Broadcast Flow

How the UI stays synchronized:

```
┌─────────────────────────────────────────────────────────────────────────┐
│ 1. web_server.cpp: loop() checks if 100ms elapsed since last broadcast  │
└─────────────────────────────────────────────────────────────────────────┘
                                    ↓
┌─────────────────────────────────────────────────────────────────────────┐
│ 2. web_server.cpp: broadcastState() builds JSON with:                   │
│    - servos[]: current positions, calibration, pins                     │
│    - wifi: mode, SSID, IP, connection status                            │
│    - eye: gaze X/Y/Z, lid positions, coupling                           │
│    - mode: current mode name (follow or auto mode name)                 │
│    - system: free heap, LED state                                       │
└─────────────────────────────────────────────────────────────────────────┘
                                    ↓ WebSocket to all clients
┌─────────────────────────────────────────────────────────────────────────┐
│ 3. app.js: onMessage() receives state, updates global `state` object    │
│    Calls updateUI() to refresh all displayed values                     │
└─────────────────────────────────────────────────────────────────────────┘
```

**Note:** Configuration data (WiFi credentials, timing settings) is NOT included in broadcast. It's fetched on-demand via `getConfig` command to prevent input field snap-back while user is typing.

### Startup Flow

```
┌─────────────────────────────────────────────────────────────────────────┐
│ 1. animatronic-eyes.ino: setup()                                        │
│    - Serial.begin()                                                     │
│    - storage.begin() → Opens NVS namespace                              │
│    - servoController.begin() → Claims LEDC timers, attaches servos      │
│    - eyeController.begin() → Applies initial gaze/lid state             │
│    - ledStatus.begin() → Configures LED PWM (uses remaining timers)     │
│    - wifiManager.begin() → Starts AP or connects to stored network      │
│    - webServer.begin() → Mounts LittleFS, starts HTTP/WebSocket         │
│    - modeManager.begin() → Loads mode files, starts default mode        │
│    - autoBlink.begin() → Starts auto-blink timer                        │
│    - impulsePlayer.begin() → Loads impulse files, preloads first        │
│    - autoImpulse.begin() → Starts auto-impulse timer                    │
│    - updateChecker.begin() → Loads config, schedules first check        │
└─────────────────────────────────────────────────────────────────────────┘
                                    ↓
┌─────────────────────────────────────────────────────────────────────────┐
│ 2. animatronic-eyes.ino: loop() - runs continuously                     │
│    - ledStatus.loop() → Updates LED blink pattern                       │
│    - wifiManager.loop() → Handles reconnection state machine            │
│    - servoController.loop() → Writes pending servo positions (throttled)│
│    - eyeController.loop() → Runs async animations (blink state machine) │
│    - modeManager.loop() → Advances mode player if in auto mode          │
│    - autoBlink.loop() → Triggers blink if interval elapsed              │
│    - impulsePlayer.loop() → Advances impulse playback state machine     │
│    - autoImpulse.loop() → Triggers impulse if interval elapsed          │
│    - updateChecker.loop() → Checks for updates if interval elapsed      │
│    - webServer.loop() → Broadcasts state every 100ms                    │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## Design Decisions

### Why Eye Controller Abstraction?

**Problem:** Direct servo control requires knowing which servo index maps to which physical function, calibration math, and vergence calculations. UI code would need hardware knowledge.

**Solution:** Eye Controller provides logical coordinates (-100 to +100) for gaze and lids. It handles:
- Mapping to correct servo indices
- Calibration scaling (min/center/max → actual range)
- Vergence calculation (eyes converge when Z is close)
- Coupling (linked vs independent eye movement)

**Benefit:** UI just says "look at X=50, Y=-30" without knowing servo wiring. Mode System and other features use the same abstraction.

### Why XOR Between Follow Mode and Mode System?

**Problem:** Both modes control gaze continuously. If both ran simultaneously, they'd fight.

**Solution:** Mutually exclusive - user picks Follow OR Auto (Mode System).

**Implementation:** Single mode selector in UI. Active mode calls `eyeController.setGaze()`. Switching modes just changes which module is updating gaze.

**Exception:** Impulses (v0.8) overlay on either mode briefly, then fade back to current mode's control.

### Why Logical Coordinates (-100 to +100)?

**Problem:** Raw servo degrees (0-180) depend on calibration and mounting. "90 degrees" means nothing without context.

**Solution:** Logical range where:
- -100 = full left/down/closed
- 0 = center/neutral
- +100 = full right/up/open

**Mapping:** `actual = center + (logical / 100.0) * (max - center)` for positive, similar for negative using min.

**Benefit:** UI and future behavior code work in intuitive coordinates. Hardware differences are hidden in calibration.

### Why Vergence and Coupling?

**Vergence:** Real eyes converge (cross) when looking at close objects. Without this, animatronic eyes look "dead" when tracking something moving toward/away.

**Formula:** `vergenceOffset = maxVergence * (100 - gazeZ) / 200`
- Z=100 (far): eyes parallel
- Z=0 (medium): slight convergence
- Z=-100 (close): maximum convergence

**Coupling:** Controls how eyes relate:
- +1 (linked): Move together with natural vergence
- 0 (independent): Parallel movement, no vergence
- -1 (divergent): "Feldman mode" - eyes diverge with vertical offset for comedic effect

### Why Config Removed from State Broadcast?

**Problem (v0.3):** State broadcast included all config. While user typed in WiFi password field, the next broadcast (200ms later) would overwrite with stored value, causing "snap-back."

**Solution (v0.4):** State broadcast only includes runtime state (positions, WiFi status). Config is fetched separately via `getConfig` command when user opens Configuration tab or needs to edit.

**Result:** User can type in fields without interference. Save button sends new values to device.

### Why Singleton Pattern for Managers?

**Pattern:** `storage`, `wifiManager`, `servoController`, `eyeController`, `ledStatus`, `webServer` are all singletons with global instances.

**Reason:**
- Only one instance makes sense (one WiFi, one servo array)
- Avoids passing references through all function calls
- Matches Arduino conventions
- Simple `wifiManager.getMode()` access from anywhere

**Trade-off:** Global state, harder to unit test. Acceptable for embedded firmware.

### Why Recovery UI Embedded in PROGMEM?

**Problem:** If LittleFS is corrupted or empty, user can't access web UI to upload new files.

**Solution:** Minimal recovery UI is compiled into firmware as PROGMEM string. Always available at `/recovery` regardless of filesystem state.

**Content:** Basic HTML with firmware upload, UI upload, factory reset. No styling dependencies.

---

## File Structure

```
animatronic-eyes/
├── animatronic-eyes.ino   # Main sketch, setup() and loop()
├── config.h               # Constants, pins, version, defaults
├── storage.h/.cpp         # NVS persistence layer
├── wifi_manager.h/.cpp    # WiFi AP/STA, mDNS, reconnection
├── servo_controller.h/.cpp # ESP32Servo wrapper, throttling
├── eye_controller.h/.cpp  # High-level eye control abstraction
├── mode_manager.h/.cpp    # Mode switching, Follow vs Auto
├── mode_player.h/.cpp     # JSON sequence interpreter
├── auto_blink.h/.cpp      # Automatic blink timer
├── impulse_player.h/.cpp  # Impulse playback with state save/restore
├── auto_impulse.h/.cpp    # Automatic impulse timer
├── update_checker.h/.cpp  # GitHub version checking
├── led_status.h/.cpp      # Status LED patterns, PWM
├── web_server.h/.cpp      # HTTP, WebSocket, OTA, recovery UI
├── data/                  # LittleFS web assets
│   ├── index.html         # Single-page app structure
│   ├── style.css          # Dark theme, responsive layout
│   ├── app.js             # WebSocket client, UI logic
│   ├── version.json       # UI version for compatibility
│   ├── modes/             # Auto mode definitions
│   │   ├── natural.json   # Calm, realistic movement
│   │   ├── sleepy.json    # Drooping eyelids, slow movements
│   │   ├── alert.json     # Wide eyes, quick scanning
│   │   ├── spy.json       # Suspicious side-glances
│   │   └── crazy.json     # Erratic, wild expressions
│   └── impulses/          # Impulse definitions
│       ├── startle.json   # Wide eyes + jerk movement
│       └── distraction.json # Quick side glance
├── docs/                  # Documentation
├── LICENSE                # CC BY-NC-SA 4.0
├── README.md              # Project overview
└── CHANGELOG.md           # Version history
```

## Module Responsibilities

### config.h

Central configuration file containing:
- `FIRMWARE_VERSION` - Current firmware version string
- `MIN_UI_VERSION` - Minimum compatible UI version
- Serial baud rate (115200)
- Default WiFi settings (AP name, password, timeouts)
- Servo count (`NUM_SERVOS = 6`)
- Servo indices (LEFT_EYE_X, LEFT_EYE_Y, etc.)
- Default GPIO pins
- Default calibration values
- `SERVO_UPDATE_INTERVAL_MS` - Throttle rate (20ms)
- WebSocket broadcast interval (100ms)
- Admin lock timeouts (unlock 15min, lockout 5min)
- Update check settings (boot delay, jitter, intervals, GitHub URLs)
- NVS namespace

### storage.h/.cpp

Persistent storage using ESP32 Preferences (NVS):
- `Storage` singleton class
- WiFi credentials (primary + secondary)
- WiFi timing settings
- AP configuration
- Servo calibration per index
- LED configuration
- mDNS settings
- Factory reset functionality

### wifi_manager.h/.cpp

WiFi connection management:
- `WifiManager` singleton class
- AP mode with auto-generated SSID
- STA mode with credentials
- Multi-SSID failover (primary → secondary → AP)
- Reconnection logic with grace period and retries
- mDNS hostname registration
- Background network scanning in AP mode

### servo_controller.h/.cpp

Low-level servo control:
- `ServoController` singleton class
- 6x ESP32Servo instances
- Position control with calibration mapping
- Invert flag support
- **Paired writes** - Up to 2 servos per 20ms tick, prioritizing pairs:
  - Eyelids (indices 2,5) - synchronized blink
  - Eye X (indices 0,3) - horizontal movement
  - Eye Y (indices 1,4) - vertical movement
- `setPositionRaw()` for calibration preview (bypasses limits)

### eye_controller.h/.cpp

High-level eye abstraction:
- `EyeController` singleton class
- `setGaze(x, y, z)` - Logical gaze coordinates (-100 to +100)
- `setLids(left, right)` - Eyelid positions (-100 to +100)
- Automatic vergence calculation based on Z depth
- Coupling parameter for eye coordination
- Vertical divergence in "Feldman mode" (coupling < 0)
- **Async blink animations** via state machine:
  - `startBlink()`, `startBlinkLeft()`, `startBlinkRight()` - Non-blocking
  - `loop()` advances animation state (CLOSING → CLOSED → OPENING → IDLE)
  - `isAnimating()` - Check if animation in progress
- `center()` - Return to neutral gaze and lids (preserves Z and coupling)
- `resetAll()` - Full reset including Z and coupling (for mode switching)
- `reapply()` - Re-send current state to servos

### led_status.h/.cpp

Status LED control:
- `LedStatus` singleton class
- Blink patterns for different states
- PWM brightness control via LEDC
- Pattern definitions: slow, double, fast, solid, strobe

### mode_manager.h/.cpp

Mode switching and coordination:
- `ModeManager` singleton class
- Manages active mode (Follow or Auto)
- `setMode(name)` - Switch modes, "follow" or auto mode name
- `getCurrentModeName()` - Returns current mode for UI
- Loads available modes from `/modes/` directory
- Resets eye state via `resetAll()` on mode switch
- Persists default mode setting

### mode_player.h/.cpp

JSON sequence interpreter for auto modes:
- `ModePlayer` singleton class
- Loads mode definitions from JSON files
- Executes mode primitives in sequence:
  - `gaze` - Set eye position (supports random ranges)
  - `lids` - Set eyelid positions
  - `blink` - Trigger blink animation
  - `wait` - Pause with optional random duration
- Supports `coupling` override per mode (negative = Feldman/divergent)
- `pause()`/`resume()` for manual control interruption
- Loops mode sequences continuously

### auto_blink.h/.cpp

Automatic blink timer:
- `AutoBlink` singleton class
- Random interval between blinks (configurable, default 2-6s)
- Works in both Follow and Auto modes
- `resetTimer()` - Called after manual blink to prevent double-blink
- `setRuntimeOverride(enabled)` - Mode can disable auto-blink
- Configurable via ModeConfig in Storage

### impulse_player.h/.cpp

Impulse playback with state save/restore:
- `ImpulsePlayer` singleton class
- Loads impulse definitions from `/impulses/` directory
- **State save/restore** - Saves gaze X/Y/Z, coupling, lids before playing, restores after
- **Preload system** - Next random impulse preloaded for instant trigger
- Executes same primitives as modes (gaze, lids, blink, wait)
- `trigger()` - Play preloaded impulse, preload next
- `isPlaying()` / `isPending()` - Check playback state
- **Precedence** - Waits for blink to finish before playing
- `stop()` - Stop playback (used for OTA safety)

### auto_impulse.h/.cpp

Automatic impulse timer:
- `AutoImpulse` singleton class
- Random interval between impulses (configurable, default 15-25s)
- Works in all modes (Follow and Auto)
- Impulse selection - Only triggers from selected subset
- `setRuntimeOverride(enabled)` - UI toggle for auto-impulse
- `getSelectedCount()` - Returns number of selected impulses
- Configurable via ImpulseConfig in Storage

### update_checker.h/.cpp

GitHub version checking:
- `UpdateChecker` singleton class
- Fetches `version.json` from GitHub raw CDN
- Compares remote version with `FIRMWARE_VERSION`
- Configurable check frequency (boot only, daily, weekly)
- 30s boot delay + random jitter (0-30 min) to prevent thundering herd
- Results cached in NVS (survives reboot)
- Cache auto-clears when firmware matches/exceeds cached version
- `checkNow()` - Manual trigger (always allowed)
- `isUpdateAvailable()` / `getAvailableVersion()` - State getters
- Only checks when WiFi STA connected (skipped in AP-only mode)

### web_server.h/.cpp

HTTP and WebSocket server:
- `WebServer` singleton class
- Static file serving from LittleFS
- WebSocket at `/ws` for real-time communication
- State broadcast every 75ms
- Command handling (see WebSocket Protocol below)
- OTA endpoints (`/update`, `/api/upload-ui`)
- Version API (`/api/version`)
- Recovery UI embedded in PROGMEM
- `WEB_LOG()` macro for dual Serial+WebSocket logging
- **Admin Lock** - IP-based authentication for protected operations:
  - PIN validation (4-6 digits)
  - 15-minute unlock timeout
  - Rate limiting (3 failures → 5-minute lockout)
  - AP clients (192.168.4.x) always unlocked
  - Per-client auth state broadcast

---

## WebSocket Protocol

### On Connect (Server → Client, once per connection)

When a client connects, the server sends available modes and impulses once:

```json
{"type": "availableModes", "modes": ["follow", "natural", "sleepy", "alert", "spy", "crazy"]}
{"type": "availableImpulses", "impulses": ["startle", "distraction"]}
```

These lists don't change at runtime, so sending them once reduces broadcast payload.

### State Broadcast (Server → Client, every 100ms)

```json
{
  "type": "state",
  "servos": [
    {
      "name": "Left Eye X",
      "pos": 90,
      "min": 45,
      "center": 90,
      "max": 135,
      "invert": false,
      "pin": 32
    }
  ],
  "wifi": {
    "mode": "STA",
    "ssid": "HomeNetwork",
    "ip": "192.168.1.100",
    "connected": true
  },
  "eye": {
    "gazeX": 0,
    "gazeY": 0,
    "gazeZ": 0,
    "lidLeft": 0,
    "lidRight": 0,
    "coupling": 1.0,
    "maxVergence": 100
  },
  "mode": {
    "current": "follow",
    "isAuto": false,
    "autoBlink": true,
    "autoBlinkActive": true
  },
  "impulse": {
    "playing": false,
    "pending": false,
    "preloaded": "startle",
    "autoImpulse": true,
    "autoImpulseActive": true,
    "selection": "startle,distraction"
  },
  "update": {
    "available": false,
    "version": "",
    "lastCheck": 0,
    "checking": false,
    "enabled": true,
    "interval": 1
  }
}
```

Mode value `current` is either `"follow"` or the auto mode name (e.g., `"natural"`, `"sleepy"`).

**Note:** Available modes/impulses are NOT in the state broadcast - they're sent once on connect.

### Commands (Client → Server)

#### Eye Controller Commands

```json
{"type": "setGaze", "x": 50, "y": -30, "z": 0}
{"type": "setLids", "left": 100, "right": 100}
{"type": "blink"}
{"type": "blink", "duration": 200}
{"type": "blinkLeft"}
{"type": "blinkRight"}
{"type": "setCoupling", "value": 1.0}
{"type": "centerEyes"}
{"type": "reapplyEyeState"}
```

Blink commands accept optional `duration` in milliseconds. If omitted (or 0), duration auto-scales based on lid position.

#### Servo Commands (Calibration)

```json
{"type": "setServo", "index": 0, "position": 90}
{"type": "previewCalibration", "index": 0, "position": 90}
{"type": "setCalibration", "index": 0, "min": 45, "center": 90, "max": 135}
{"type": "setPin", "index": 0, "pin": 32}
{"type": "setInvert", "index": 0, "invert": true}
{"type": "centerAll"}
```

#### Configuration Commands

```json
{"type": "getConfig"}
{"type": "setWifiNetwork", "index": 0, "ssid": "...", "password": "..."}
{"type": "clearWifiNetwork", "index": 0}
{"type": "setWifiTiming", "grace": 3, "retries": 3, "interval": 10, "apScan": 5}
{"type": "setKeepAP", "enabled": true}
{"type": "setAp", "prefix": "LookIntoMyEyes", "password": "12345678"}
{"type": "setMdns", "enabled": true, "hostname": "animatronic-eyes"}
{"type": "setLed", "enabled": true, "pin": 2, "brightness": 255}
{"type": "scanNetworks"}
{"type": "resetConnection"}
```

#### System Commands

```json
{"type": "reboot"}
{"type": "factoryReset"}
```

#### Mode System Commands

```json
{"type": "setMode", "mode": "follow"}
{"type": "setMode", "mode": "natural"}
{"type": "getModeConfig"}
{"type": "setModeConfig", "defaultMode": "follow", "autoBlinkEnabled": true, "blinkIntervalMin": 2000, "blinkIntervalMax": 6000}
{"type": "listModes"}
{"type": "setAutoBlinkOverride", "enabled": true}  // Runtime override for Control tab toggle
```

#### Impulse System Commands

```json
{"type": "triggerImpulse"}
{"type": "setAutoImpulse", "enabled": true}         // Persists to config
{"type": "setAutoImpulseOverride", "enabled": true} // Runtime override for Control tab toggle
{"type": "setImpulseInterval", "min": 15000, "max": 25000}
{"type": "setImpulseSelection", "selection": ["startle", "distraction"]}
{"type": "getAvailableImpulses"}
{"type": "getImpulseConfig"}
{"type": "setImpulseConfig", "autoImpulse": true, "intervalMin": 15000, "intervalMax": 25000, "selection": ["startle"]}
```

#### Update Check Commands

```json
{"type": "checkForUpdate"}
{"type": "setUpdateCheckEnabled", "enabled": true}
{"type": "setUpdateCheckInterval", "interval": 1}
```

- `checkForUpdate` - Always allowed (read-only)
- `setUpdateCheckEnabled` - Protected (requires admin unlock)
- `setUpdateCheckInterval` - Protected (requires admin unlock)
- Interval values: 0 = boot only, 1 = daily, 2 = weekly

#### Admin Lock Commands

```json
{"type": "adminAuth", "pin": "1234"}
{"type": "adminLock"}
{"type": "setAdminPin", "pin": "1234"}
{"type": "clearAdminPin"}
```

Server responds with `adminState` message:

```json
{
  "type": "adminState",
  "locked": true,
  "isAPClient": false,
  "pinConfigured": true,
  "remainingSeconds": 845,
  "lockoutSeconds": 0
}
```

- `locked`: true if PIN configured and not unlocked
- `isAPClient`: true if connected via AP (always unlocked)
- `remainingSeconds`: seconds until auto-lock (0 if locked)
- `lockoutSeconds`: seconds remaining in rate limit lockout (0 if not locked out)

### Log Messages (Server → Client)

```json
{"type": "logLine", "line": "[WiFi] Connected to HomeNetwork"}
```

---

## Version Compatibility System

### Version Files

| File | Field | Purpose |
|------|-------|---------|
| `config.h` | `FIRMWARE_VERSION` | Current firmware version |
| `config.h` | `MIN_UI_VERSION` | Minimum UI version required |
| `data/version.json` | `version` | Current UI version |
| `data/version.json` | `minFirmware` | Minimum firmware required |

### Compatibility Check

The `/api/version` endpoint returns:

```json
{
  "firmware": "0.9.17",
  "minUiVersion": "0.9.17",
  "uiVersion": "0.9.17",
  "uiMinFirmware": "0.9.17",
  "uiStatus": "ok",
  "deviceId": "ABC123",
  "chipModel": "ESP32-D0WD-V3",
  "chipRevision": 3
}
```

### Status Values

| Status | Meaning | UI Indicator |
|--------|---------|--------------|
| `ok` | Versions compatible | Green |
| `minor_mismatch` | Minor version differs | Yellow |
| `major_mismatch` | Major version differs | Red |
| `fw_too_old` | Firmware older than UI requires | Red |
| `ui_too_old` | UI older than firmware requires | Red |
| `missing` | version.json not found | Red |

---

## Memory Map

### Flash Partitions (Default 4MB with SPIFFS)

| Partition | Offset | Size | Purpose |
|-----------|--------|------|---------|
| nvs | 0x9000 | 20KB | Preferences storage |
| otadata | 0xE000 | 8KB | OTA state |
| app0 | 0x10000 | 1.2MB | Main application |
| app1 | 0x150000 | 1.2MB | OTA update slot |
| spiffs | 0x290000 | 1.5MB | LittleFS filesystem |

### RAM Usage

- Free heap at boot: ~289KB
- Free heap after init: ~212KB
- WebSocket buffer: ~4KB per client
- JSON document: ~2KB static buffer

---

## Libraries Used

| Library | Version/Source | Purpose |
|---------|----------------|---------|
| WiFi.h | ESP32 core | WiFi AP/STA |
| ESPAsyncWebServer | ESP32Async | Async HTTP |
| AsyncTCP | ESP32Async | TCP backend |
| ArduinoJson | 7.x | JSON parsing |
| ESP32Servo | Kevin Harrington | PWM servo |
| Preferences | ESP32 core | NVS storage |
| LittleFS | ESP32 core | Filesystem |
| Update | ESP32 core | OTA updates |
| ESPmDNS | ESP32 core | mDNS/Bonjour |

---

## Version History

| Version | Feature | Details |
|---------|---------|---------|
| v1.0 | Update Check | GitHub version checking, configurable schedule |
| v0.9 | Admin Lock | PIN protection, IP-based auth, rate limiting |
| v0.8 | Impulses | One-shot animations with state save/restore |
| v0.7 | Mode System | JSON-based autonomous behaviors, auto-blink |
| v0.6 | Follow Mode | Manual gaze control via gaze pad |
| v0.5 | Eye Controller | Gaze abstraction, vergence, coupling |

See [CHANGELOG.md](../CHANGELOG.md) for detailed version history.
