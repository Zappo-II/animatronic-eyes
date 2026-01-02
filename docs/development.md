# Development Guide

Guide for contributors and developers who want to modify or extend the project.

## Development Environment

### Required Tools

- Arduino IDE 2.x (recommended) or 1.8.x
- ESP32 board support package
- Required libraries (see [Setup Guide](setup.md#step-3-install-required-libraries))
- Serial monitor (built into Arduino IDE)
- Web browser with developer tools

### Optional Tools

- VS Code with Arduino extension
- esptool.py for direct flash operations
- mklittlefs for building UI images

## Code Conventions

### C++ (Firmware)

- Singletons for manager classes (e.g., `storage`, `wifiManager`, `webServer`)
- Prefix private members with underscore: `_privateVar`
- Use `uint8_t`, `uint32_t` etc. for specific sizes
- Constants in `config.h`, not scattered through code
- Always check array bounds before access

### JavaScript (Web UI)

- ES6+ features (const/let, arrow functions, template literals)
- Single `app.js` file (no build step)
- WebSocket message handlers grouped together
- Throttle user inputs to prevent flooding

### CSS

- CSS custom properties for theming
- Mobile-first responsive design
- Dark theme as default

## Serial Log Prefixes

Use consistent prefixes for log messages:

| Prefix | Module |
|--------|--------|
| `[Storage]` | NVS operations |
| `[Servo]` | Servo attach/position |
| `[Eye]` | Eye Controller |
| `[Mode]` | Mode Manager/Player |
| `[AutoBlink]` | Automatic blink system |
| `[Impulse]` | Impulse playback |
| `[AutoImpulse]` | Automatic impulse system |
| `[Control]` | Manual control events (blink, wink) |
| `[WiFi]` | Connection state |
| `[WebServer]` | HTTP init |
| `[WS]` | WebSocket events/commands |
| `[OTA]` | Firmware updates |
| `[LED]` | Status LED |
| `[Admin]` | Admin authentication events |

### WEB_LOG Macro

For messages that should appear in both Serial and Web Console:

```cpp
WEB_LOG("WiFi", "Connected to %s", ssid);
// Output: [WiFi] Connected to HomeNetwork
```

## Adding a New Feature

### 1. Plan the Feature

- What module(s) does it affect?
- Does it need persistent storage?
- Does it need WebSocket commands?
- Does it need UI changes?

### 2. Update config.h

Add any new constants:

```cpp
// config.h
#define NEW_FEATURE_DEFAULT 100
```

### 3. Add Storage (if needed)

```cpp
// storage.h
struct NewFeatureConfig {
    int value;
};

NewFeatureConfig getNewFeatureConfig();
void setNewFeatureConfig(const NewFeatureConfig& config);

// storage.cpp - implement with Preferences
```

### 4. Add WebSocket Command

```cpp
// web_server.cpp in handleWebSocketMessage()
else if (type == "setNewFeature") {
    int value = doc["value"];
    // Handle the command
    WEB_LOG("WS", "setNewFeature: value=%d", value);
}
```

### 5. Update State Broadcast (if needed)

```cpp
// web_server.cpp in broadcastState()
JsonObject newFeature = doc.createNestedObject("newFeature");
newFeature["value"] = getNewFeatureValue();
```

### 6. Update UI

```html
<!-- index.html -->
<div class="control">
    <label>New Feature</label>
    <input type="range" id="newFeatureSlider" min="0" max="100">
</div>
```

```javascript
// app.js
const newFeatureSlider = document.getElementById('newFeatureSlider');
newFeatureSlider.addEventListener('input', () => {
    sendCommand({ type: 'setNewFeature', value: parseInt(newFeatureSlider.value) });
});
```

### 7. Update Version

If your change affects compatibility:

```cpp
// config.h
#define FIRMWARE_VERSION "0.5.18"
// Update MIN_UI_VERSION if UI changes are required
```

```json
// data/version.json
{
    "version": "0.5.18",
    "minFirmware": "0.5.0"
}
```

### 8. Update Documentation

- Add to CHANGELOG.md
- Update relevant docs/ files

## OTA Firmware Update Workflow

For iterating without USB cable:

1. Make code changes
2. Update `FIRMWARE_VERSION` in config.h
3. **Sketch → Export Compiled Binary**
4. Find the .bin file in sketch folder
5. Upload via web UI: Configuration → System → Upload Firmware

## UI File Update Workflow

For updating web interface:

1. Edit files in `data/` folder
2. Update version in `data/version.json`
3. Create LittleFS image:

```bash
# Find your mklittlefs path
~/.arduino15/packages/esp32/tools/mklittlefs/*/mklittlefs \
    -c data/ \
    -p 256 \
    -b 4096 \
    -s 0x160000 \
    ui.bin
```

Note: Size `0x160000` (1.44MB) matches the "Default 4MB with spiffs" partition scheme.

4. Upload via one of:

**Web UI:**
```bash
curl -X POST -F "file=@ui.bin" http://<device-ip>/api/upload-ui
```

**Note**: If Admin PIN is configured, unlock first (15-minute session):
```bash
curl -X POST -H "Content-Type: application/json" -d '{"pin":"1234"}' http://<device-ip>/api/unlock
```
Or use the AP address (`http://192.168.4.1/...`) which bypasses authentication.

**esptool (USB):**
```bash
~/.arduino15/packages/esp32/tools/esptool_py/*/esptool \
    --chip esp32 \
    --port /dev/ttyUSB0 \
    --baud 921600 \
    write_flash 0x290000 ui.bin
```

**Arduino IDE:**
- Ctrl+Shift+P → "Upload LittleFS"

## WebSocket Debugging

### Serial Monitor

Commands are logged with details:
```
[WS] Command: setGaze
[WS] setGaze: x=50, y=-30, z=0
```

### Browser Console

Open developer tools (F12) and check:
- Network tab → WS → Messages for WebSocket traffic
- Console for JavaScript errors

### Test Commands

Send test commands from browser console:
```javascript
ws.send(JSON.stringify({type: 'centerEyes'}));
```

## Common Pitfalls

### Watchdog Crashes

The ESP32 Task Watchdog Timer will reset the device if loop() takes too long.

**Avoid:**
- Blocking delays in loop()
- Rapid servo writes (use ServoController throttling)
- Long-running operations without yield()

**Solution:** Servo writes are throttled to 20ms intervals via `SERVO_UPDATE_INTERVAL_MS`.

### WiFi Mode Conflicts

`WiFi.disconnect(true)` can reset the WiFi mode.

**Solution:** Always set `WiFi.mode(WIFI_AP_STA)` before `WiFi.begin()`.

### NVS Write Failures

Preferences writes can fail silently.

**Solution:** Verify writes by reading back:
```cpp
prefs.putString("key", value);
String readBack = prefs.getString("key", "");
if (readBack != value) {
    // Handle failure
}
```

### LEDC Timer Conflicts

ESP32 LEDC timers are shared between PWM channels. Servo library and LED PWM can conflict.

**Solution:** Initialize servos before LED to let servos claim their timers first.

### JSON Buffer Overflow

ArduinoJson documents have fixed size.

**Solution:** Use static buffers with adequate size:
```cpp
StaticJsonDocument<2048> doc;
```

## Creating Custom Modes

Modes are JSON files stored in `data/modes/`. Each mode defines an autonomous behavior sequence.

### Mode File Structure

```json
{
  "name": "CustomMode",
  "loop": true,
  "coupling": 1.0,
  "sequence": [
    {"gaze": {"x": 0, "y": 0, "z": 50}},
    {"wait": 2000},
    {"lids": {"left": 50, "right": 50}},
    {"blink": 150}
  ]
}
```

### Mode Properties

| Property | Type | Description |
|----------|------|-------------|
| `name` | string | Display name in mode selector |
| `loop` | boolean | Whether to repeat sequence |
| `coupling` | float | Eye coupling (-1 to +1, negative = Feldman/divergent) |
| `sequence` | array | List of primitives to execute |

### Primitive Types

#### gaze - Set Eye Position
```json
{"gaze": {"x": 50, "y": -30, "z": 0}}
```

- `x`: Horizontal (-100 to +100, left to right)
- `y`: Vertical (-100 to +100, down to up)
- `z`: Depth (-100 to +100, close to far)

#### lids - Set Eyelid Position
```json
{"lids": {"left": 100, "right": 100}}
```

- `left`/`right`: -100 (closed) to +100 (open)

#### blink - Trigger Blink
```json
{"blink": 150}
```

Value is duration in milliseconds.

#### wait - Pause Sequence
```json
{"wait": 2000}
```

Value is duration in milliseconds.

### Random Values

Any numeric parameter in any primitive supports random ranges using `{"random": [min, max]}`:

```json
{"gaze": {"x": {"random": [-80, 80]}, "y": {"random": [-30, 30]}, "z": {"random": [0, 50]}}}
{"lids": {"left": {"random": [-50, 100]}, "right": {"random": [-50, 100]}}}
{"blink": {"random": [80, 200]}}
{"wait": {"random": [1000, 3000]}}
```

A new random value is generated each time the step executes (on each loop iteration for modes).

### Example: Nervous Mode

```json
{
  "name": "Nervous",
  "loop": true,
  "coupling": 1.0,
  "sequence": [
    {"gaze": {"x": {"random": [-60, 60]}, "y": {"random": [-20, 20]}}},
    {"wait": {"random": [200, 500]}},
    {"gaze": {"x": {"random": [-60, 60]}, "y": {"random": [-20, 20]}}},
    {"wait": {"random": [150, 400]}},
    {"blink": 80},
    {"wait": {"random": [500, 1500]}},
    {"gaze": {"x": 0, "y": 0}},
    {"wait": 300},
    {"gaze": {"x": {"random": [-80, 80]}, "y": 0}},
    {"wait": {"random": [100, 300]}}
  ]
}
```

### Deploying Custom Modes

1. Create JSON file in `data/modes/` (e.g., `nervous.json`)
2. Upload UI files via Arduino IDE (Ctrl+Shift+P → "Upload LittleFS")
3. Or rebuild and upload `ui.bin` via web interface
4. Mode appears automatically in mode selector dropdown

### Tips

- Keep sequences relatively short (10-20 steps) for variety
- Use random ranges for natural-feeling behavior
- Test with small wait times first, then adjust
- Negative coupling creates "Feldman mode" (eyes diverge)
- Modes with fast movements may need higher blink intervals

## Creating Custom Impulses

Impulses are JSON files stored in `data/impulses/`. Each impulse defines a one-shot animation that interrupts the current state and restores it afterward.

### Impulse File Structure

```json
{
  "name": "CustomImpulse",
  "restore": true,
  "sequence": [
    {"lids": {"left": 100, "right": 100}},
    {"gaze": {"x": {"random": [-30, 30]}, "y": 20}},
    {"wait": 200}
  ]
}
```

### Impulse Properties

| Property | Type | Description |
|----------|------|-------------|
| `name` | string | Display name in impulse selection |
| `restore` | boolean | Must be `true` for impulses (triggers state save/restore) |
| `sequence` | array | List of primitives to execute |

### Primitive Types

Impulses use the same primitives as modes:

- `gaze` - Set eye position (supports random ranges)
- `lids` - Set eyelid positions
- `blink` - Trigger blink animation
- `wait` - Pause (supports random duration)

### Example: Double-Take Impulse

```json
{
  "name": "DoubleTake",
  "restore": true,
  "sequence": [
    {"gaze": {"x": -60, "y": 0}},
    {"wait": 100},
    {"gaze": {"x": 0, "y": 0}},
    {"wait": 150},
    {"gaze": {"x": -60, "y": 0}},
    {"lids": {"left": 80, "right": 80}},
    {"wait": 300}
  ]
}
```

### Deploying Custom Impulses

1. Create JSON file in `data/impulses/` (e.g., `doubletake.json`)
2. Upload UI files via Arduino IDE (Ctrl+Shift+P → "Upload LittleFS")
3. Or rebuild and upload `ui.bin` via web interface
4. Impulse appears automatically in Impulse Settings selection

### Tips

- Keep impulses short (under 1 second total) for snappy reactions
- Use `restore: true` - this is what makes it an impulse vs a mode
- State is saved before playback and restored after
- Impulses wait for any in-progress blink to finish before playing
- Test with the manual Impulse button before enabling auto-impulse

## Testing Checklist

Before submitting changes:

- [ ] Compiles without warnings
- [ ] Upload succeeds
- [ ] Serial output shows expected startup
- [ ] Web UI loads correctly
- [ ] New feature works as expected
- [ ] Existing features still work
- [ ] Mobile layout not broken
- [ ] Recovery mode still accessible
- [ ] Factory reset still works
- [ ] Version numbers updated if needed
- [ ] CHANGELOG.md updated

## Recovery Mode

The recovery UI is embedded in firmware PROGMEM, so it's always available even if LittleFS is corrupted.

Access via: `http://<device-ip>/recovery`

Recovery provides:
- System info
- Firmware upload
- UI file upload
- Download backup
- Factory reset
- Wipe UI files

### Admin Lock in Recovery Mode

If Admin Lock is enabled, protected actions (uploads, backup, wipe) require authentication:
- Enter PIN in the lock banner at the top of the page
- AP clients (192.168.4.x) are always unlocked
- Factory reset is always allowed (escape hatch for forgotten PIN)

## Build Stats Reference

Typical compile output:
```
Sketch uses 1088391 bytes (83%) of program storage space.
Global variables use 45312 bytes (13%) of dynamic memory.
```

If you exceed 1.2MB (app partition size), consider:
- Removing unused features
- Moving strings to PROGMEM
- Optimizing embedded recovery UI
