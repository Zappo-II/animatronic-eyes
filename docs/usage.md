# Usage Guide

Complete guide to using the Animatronic Eyes web interface.

## Accessing the Web Interface

### Via Access Point (AP Mode)

When the device has no WiFi configured, or can't connect to a network:

1. Connect to WiFi: **LookIntoMyEyes-XXXXXX** (password: `12345678`)
2. Open browser: http://192.168.4.1

### Via Home Network (STA Mode)

When connected to your WiFi network:

1. Use the IP address shown in Serial Monitor, or
2. Use mDNS: http://animatronic-eyes-XXXXXX.local (if mDNS is enabled)

---

## Header Bar

The header shows device status at a glance:

| Indicator | Meaning |
|-----------|---------|
| **AP** (green) | Access Point is active |
| **WiFi** (green) | Connected to WiFi network |
| **WiFi** (yellow) | Connecting/reconnecting |
| **UI** (green) | UI version compatible |
| **UI** (yellow) | Minor version mismatch |
| **UI** (red) | Major version mismatch - update recommended |
| **Reboot** (yellow, flashing) | Reboot required for changes to take effect |
| **⬆️** (yellow, pulsing) | Firmware update available - click to open releases |

Click the **info icon (i)** to view the About section with version and device details.

---

## Control Tab

The main interface for controlling the eyes.

### Eye Preview

A live visualization showing:
- Current eye position (gaze direction)
- Eyelid state (open/closed)
- Vergence effect (eyes converging when looking "close")

### Gaze Pad

Touch or click and drag to control where the eyes look:
- **Horizontal (X)**: Left to right (-100 to +100)
- **Vertical (Y)**: Down to up (-100 to +100)

The current X/Y values are displayed above the pad.

### Depth Slider (Z)

Controls the perceived depth the eyes are looking at:
- **-100**: Looking far away (eyes parallel)
- **0**: Medium distance
- **+100**: Looking very close (eyes cross/converge)

### Coupling Slider

Controls how the eyes move relative to each other:
- **+1 (Linked)**: Eyes move together with natural vergence
- **0 (Independent)**: Eyes move in parallel, no vergence
- **-1 (Divergent)**: "Feldman mode" - eyes diverge, with vertical offset

### Lid Controls

Two sliders control the eyelids:
- **-100**: Fully closed
- **0**: Neutral/center position
- **+100**: Fully open

#### Link Toggle (Chain Icon)

When linked (default): moving either slider moves both lids together.
When unlinked: control each eyelid independently.

#### Blink / Wink Buttons

- **Blink**: Both eyes blink simultaneously
- **Wink L / Wink R**: Single eye blink

### Center Button

Resets gaze to center (X=0, Y=0) and lids to neutral (0). Does not reset depth (Z) or coupling.

---

## Mode System

The mode selector dropdown lets you choose between manual control and autonomous behaviors.

### Follow Mode

Manual control via the gaze pad and sliders. The eyes stay where you put them.

- Use the gaze pad to direct eye movement
- Adjust depth (Z) and coupling as desired
- Control eyelids manually with sliders
- Auto-blink keeps eyes looking natural (if enabled)

### Auto Modes

Autonomous behaviors defined as JSON sequences. The eyes follow pre-programmed patterns.

| Mode | Description |
|------|-------------|
| **Natural** | Calm, realistic eye movement with occasional glances |
| **Sleepy** | Drooping eyelids, slow movements, occasional dozing |
| **Alert** | Wide eyes, quick scanning movements |
| **Spy** | Suspicious side-glances and narrowed lids |
| **Crazy** | Erratic movement, rapid direction changes, wild expressions |

### Manual Control During Auto Modes

You can temporarily override auto modes:

- **Gaze pad**: Touch pauses the mode briefly, then it resumes
- **Lid sliders**: Manual lid control pauses the mode
- **Blink/Wink buttons**: Trigger immediate blink, mode continues
- **Impulse button**: Trigger immediate random impulse, mode continues

Switching modes always resets eye state to neutral (including Z and coupling).

### Custom Modes

You can create your own autonomous behaviors by uploading JSON files to the device. Custom modes appear alongside built-in modes in the dropdown. See [Development Guide](development.md#creating-custom-modes) for detailed examples.

#### Mode JSON Format

```json
{
  "name": "My Custom Mode",
  "loop": true,
  "coupling": 1.0,
  "sequence": [
    {"gaze": {"x": 0, "y": 0, "z": 50}},
    {"wait": 2000},
    {"gaze": {"x": {"random": [-30, 30]}, "y": {"random": [-20, 20]}}},
    {"wait": {"random": [1000, 3000]}},
    {"blink": 150}
  ]
}
```

#### Primitives

| Primitive | Parameters | Description |
|-----------|------------|-------------|
| `gaze` | `x`, `y`, `z` (-100 to +100) | Set eye position |
| `lids` | `left`, `right` (-100 to +100) | Set eyelid positions |
| `blink` | duration in ms (0 = auto-scale) | Trigger a blink |
| `wait` | milliseconds | Pause before next step |

#### Random Values

Any numeric value can be randomized:
```json
{"x": {"random": [-30, 30]}}
{"wait": {"random": [1000, 3000]}}
```

#### Coupling

The `coupling` parameter controls eye coordination:
- `1.0` - Eyes move together with natural vergence
- `0.0` - Eyes move independently
- `-1.0` - "Feldman mode" - eyes diverge

#### Uploading Custom Modes

1. Create a JSON file following the format above
2. Include it in the `data/modes/` folder
3. Upload via "Upload UI Files" in Configuration tab

Alternatively, use Backup/Restore to include custom modes in your backup file.

### Auto-Blink

Automatic blinking works in both Follow and Auto modes:

- Configurable interval (default: 2-6 seconds between blinks)
- Manual blinks reset the auto-blink timer (prevents double-blink)
- Can be disabled in mode settings

---

## Impulse System

Impulses are one-shot animations that briefly interrupt the current state, then restore it. They work in both Follow and Auto modes.

### Control Tab Buttons

The Control tab includes impulse buttons alongside blink controls:

| Button | Function |
|--------|----------|
| **Auto-I** | Toggle automatic impulses on/off |
| **Impulse** | Trigger a random impulse immediately |

Both buttons are disabled if no impulses are selected in settings.

### Built-in Impulses

| Impulse | Effect |
|---------|--------|
| **Startle** | Wide eyes + quick random jerk movement (200ms) |
| **Distraction** | Quick side glance then return (150-300ms) |

### How Impulses Work

1. Current state (gaze, lids, coupling) is saved
2. Impulse animation plays
3. Original state is restored

### Precedence Rules

- **Impulse during blink**: Waits for blink to finish, then plays
- **Blink during impulse**: Blink is skipped
- **Auto-blink timer during impulse**: Skipped, timer resets after

### Auto-Impulse

Like auto-blink, auto-impulse triggers random impulses at configurable intervals:

- Default interval: 15-25 seconds
- Only triggers impulses from your selected subset
- Works in all modes (Follow and Auto)
- Can be toggled via Auto-I button or in settings

### Custom Impulses

Create your own reaction animations by uploading JSON files. Custom impulses appear in the Impulse Selection checkboxes. See [Development Guide](development.md#creating-custom-impulses) for detailed examples.

#### Impulse JSON Format

```json
{
  "name": "Surprise",
  "restore": true,
  "sequence": [
    {"lids": {"left": 100, "right": 100}},
    {"gaze": {"x": {"random": [-30, 30]}, "y": {"random": [10, 30]}}},
    {"wait": 200}
  ]
}
```

Key difference from modes: use `"restore": true` instead of `"loop": true`. This saves the current state before playing and restores it after.

#### Primitives

Same as modes:

| Primitive | Parameters | Description |
|-----------|------------|-------------|
| `gaze` | `x`, `y`, `z` (-100 to +100) | Set eye position |
| `lids` | `left`, `right` (-100 to +100) | Set eyelid positions |
| `blink` | duration in ms (0 = auto-scale) | Trigger a blink |
| `wait` | milliseconds | Pause before next step |

#### Uploading Custom Impulses

1. Create a JSON file following the format above
2. Include it in the `data/impulses/` folder
3. Upload via "Upload UI Files" in Configuration tab

Alternatively, use Backup/Restore to include custom impulses in your backup file.

**Tip**: After uploading, enable your custom impulse in Impulse Selection to include it in auto-impulse and the random trigger button.

---

## Calibration Tab

Fine-tune servo positions for proper eye movement.

### Servo Cards

Each servo has a card with:
- **Name**: Which servo (Left Eye X, Right Eyelid, etc.)
- **Pin**: GPIO pin number (configurable)
- **Min/Center/Max**: Calibration range in degrees (0-180)
- **Invert**: Reverse servo direction (takes effect immediately, moves servo to center)
- **Test Slider**: Move servo to any position within its range

### Calibration Process

See [Calibration Guide](calibration.md) for detailed instructions.

### Center All Button

Moves all servos to their calibration center position. Useful for checking neutral alignment.

---

## Configuration Tab

Device settings organized in collapsible sections.

### WiFi Status

Shows current connection info:
- Network name
- IP address
- mDNS hostname (if enabled)

### WiFi Primary / Secondary

Configure up to two WiFi networks:
- **Scan**: Find available networks
- **SSID/Password**: Network credentials
- **Connect**: Save and attempt connection
- **Forget**: Clear saved credentials

The device tries Primary first, then Secondary, before falling back to AP mode.

### AP Settings

Configure the Access Point:
- **SSID Prefix**: Name shown when in AP mode (default: LookIntoMyEyes)
- **Password**: AP password (default: 12345678)

Changes require a reboot to take effect.

### WiFi Timing

Fine-tune WiFi behavior:
- **Grace Period**: How long to wait before reconnecting (1-10s)
- **Retry Attempts**: Connection attempts per network (1-10)
- **Retry Interval**: Delay between attempts (5-60s)
- **AP Scan Interval**: How often to check for networks while in AP mode (1-30 min)
- **Keep AP Active**: Whether to keep AP running when connected to WiFi

### mDNS

Enable device discovery via `.local` hostname:
- **Enabled**: Toggle mDNS on/off
- **Hostname**: Base name (device ID is always appended)

Example: hostname `animatronic-eyes` becomes `animatronic-eyes-ABC123.local`

### LED

Configure the status LED:
- **Enabled**: Toggle LED on/off
- **Pin**: GPIO pin for LED
- **Brightness**: 1-255 (PWM dimming)

### Mode Settings

Configure autonomous behavior defaults:
- **Startup Mode**: Which mode to start in after reboot (Follow or any Auto mode)
- **Remember last active mode**: When checked, the startup mode automatically updates whenever you switch modes in the Control tab. The dropdown becomes read-only and shows the current mode. Uncheck to manually set a specific startup mode.
- **Auto-Blink**: Enable/disable automatic blinking
- **Blink Interval**: Range for random blink timing (default: 2-6 seconds)

### Impulse Settings

Configure impulse behavior:
- **Auto-Impulse**: Enable/disable automatic random impulses
- **Impulse Interval**: Range for random impulse timing (default: 15-25 seconds)
- **Impulse Selection**: Checkboxes to select which impulses are used for auto/random triggers

At least one impulse must be selected for auto-impulse and the manual Impulse button to work.

### Backup / Restore

Manage device configuration:
- **Download Backup**: Save complete device configuration as JSON file
- **Restore Backup**: Upload a previously saved backup to restore settings

Backups include: calibration, WiFi settings, mode settings, impulse settings, custom mode files, and custom impulse files.

**Note**: Admin PIN is not included in backups for security reasons.

### Admin Lock

Protect configuration changes with a PIN:

- **PIN**: Set a 4-6 digit PIN to lock configuration
- **Unlock**: Click the connection status bar when locked to enter PIN
- **Clear PIN**: Remove PIN protection (requires unlock first)

When locked:
- Calibration and Configuration tabs show lock icons
- Protected controls are disabled (inputs, sliders, buttons)
- Reboot is always allowed (unless rate limited)

When unlocked:
- 15-minute timeout with countdown shown in status bar
- Connection bar shows wrench icon (🛠️) instead of lock (🔒)

**AP clients are always unlocked**: Connecting via the device's own AP (192.168.4.1) bypasses PIN protection entirely.

**Rate limiting**: After 3 failed PIN attempts, you're locked out for 5 minutes. During lockout, the reboot button is also disabled to prevent bypass.

**Note**: Admin PIN is not included in backups and must be configured separately on each device.

### Update Check

Check for newer firmware versions on GitHub:

- **Check for updates**: Enable/disable automatic version checking
- **Frequency**: How often to check (On startup only, Daily, Weekly)
- **Check Now**: Manual check button (always allowed, even when locked)

When an update is available:
- Yellow pulsing ⬆️ indicator appears in header
- Click the indicator to open GitHub releases page
- Status shown in the Update Check section

**Schedule**: Checks occur at boot (after 30s delay) and then at the configured interval. Random jitter (0-30 min) is added to prevent all devices checking at the same time.

**Note**: Update checking requires WiFi connection to your home network. It does not work in AP-only mode.

### System

- **Upload Firmware**: OTA update via .bin file
- **Upload UI Files**: Update web interface via LittleFS image
- **Reboot**: Restart the device (disabled when rate limited)
- **Factory Reset**: Clear all settings and return to defaults

### About

Device information:
- Firmware and UI versions
- ESP32 chip model and revision
- Device ID
- Project credits and license

---

## Console Tab

Real-time log viewer for debugging.

### Features

- Live log streaming via WebSocket
- Color-coded by log source:
  - **Cyan**: WiFi events
  - **Purple**: WebSocket events
  - **Green**: OTA updates
  - **Orange**: AutoBlink, AutoImpulse events
  - **Blue**: Control events (blink, wink)
  - **Magenta**: Impulse playback
  - **Red**: Errors
  - **Yellow**: Warnings
- 50-line history buffer
- Auto-scroll toggle
- Clear button

### Common Log Messages

| Message | Meaning |
|---------|---------|
| `[AutoBlink] Auto-triggered blink` | Automatic blink fired |
| `[Control] Blink` | Manual blink button pressed |
| `[Control] Wink left/right` | Wink button pressed |
| `[AutoImpulse] Auto-triggered impulse` | Automatic impulse fired |
| `[Impulse] Playing: Startle` | Impulse animation started |
| `[Admin] IP x.x.x.x unlocked` | Successful PIN entry |
| `[Admin] IP x.x.x.x locked` | Manual lock or timeout |
| `[Admin] Auth failed: wrong PIN` | Incorrect PIN entered |
| `[Admin] Rate limit: x.x.x.x locked out` | Too many failed attempts |

---

## Recovery Mode

If the web UI files are missing or corrupted, access the embedded recovery UI:

**http://[device-ip]/recovery**

Recovery mode provides:
- System information
- Firmware upload
- UI file upload
- Download backup (save configuration before reset)
- Factory reset
- Wipe UI files (to trigger fresh upload)

The recovery UI is embedded in firmware, so it's always available.

### Admin Lock in Recovery Mode

If Admin Lock is enabled, protected actions (uploads, backup, wipe) are disabled until you unlock:
- Enter your PIN in the lock banner at the top
- AP clients (192.168.4.1) are always unlocked
- After firmware upload and reboot, you can unlock from recovery UI before uploading new UI files

---

## LED Status Patterns

The status LED (if enabled) indicates device state:

| Pattern | Meaning |
|---------|---------|
| Slow blink (1s) | AP mode, no WiFi configured |
| Double blink | AP mode, fell back from WiFi |
| Fast blink (200ms) | Connecting to WiFi |
| Solid on | Connected to WiFi |
| Very fast (100ms) | OTA update in progress |
| Strobe (50ms) | Factory reset in progress |

---

## Tips

### Mobile Usage

- Sliders have touch-safe zones on the right edge for scrolling
- Tap status indicators to see tooltips
- Use landscape orientation for the gaze pad

### Performance

- The device broadcasts state every 100ms via WebSocket
- Servo movements are throttled to prevent watchdog crashes
- Close other browser tabs to reduce WebSocket reconnection attempts
