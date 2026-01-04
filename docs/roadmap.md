# Roadmap

Detailed specifications for planned features.

---

## ✅ v1.0 - Update Check

Check if newer version is available on GitHub. No automatic download - just notification.

### Features

- [x] Fetch `https://raw.githubusercontent.com/Zappo-II/animatronic-eyes/main/data/version.json`
- [x] Compare `version` field with running `FIRMWARE_VERSION`
- [x] Cache result in NVS (persists across reboots)
- [x] Configurable check frequency: boot only, daily (default), weekly
- [x] Check at boot (after 30s delay) + periodic based on interval
- [x] Manual "Check Now" button in System section (always allowed)

### Safe Practices (GitHub Rate Limits)

- [x] Use `raw.githubusercontent.com` (CDN-cached, more lenient than API)
- [x] Cache aggressively in NVS
- [x] Random jitter (0-30 min) added to interval to avoid thundering herd
- [x] Silent fail on error - retry next interval
- [x] No retry loop on failure - single attempt per check
- [x] Only check when WiFi STA connected (skip in AP-only mode)

### Header Indicator (when update available)

```
[AP] [WiFi] [UI] [⬆️] [Reboot]
```

- [x] Yellow pulsing ⬆️ indicator
- [x] Tooltip on hover/tap: "Update available: v1.0.1"
- [x] Click opens GitHub releases page in new tab

### System Section - Update Check Card

- [x] Enable/disable checkbox (admin protected)
- [x] Frequency dropdown: On startup only, Daily, Weekly (admin protected)
- [x] Check Now button (always allowed)
- [x] Status display: checking, up to date, update available with link

### Admin Lock

- [x] `checkForUpdate` - Always allowed (read-only action)
- [x] `setUpdateCheckEnabled` - Protected (requires admin unlock)
- [x] `setUpdateCheckInterval` - Protected (requires admin unlock)

### Recovery UI

- [x] Shows cached update status if available
- [x] Version links to GitHub releases

### File Changes

- [x] `config.h` - Update check constants, GitHub URLs
- [x] `storage.h/.cpp` - UpdateCheckConfig and UpdateCheckCache structs
- [x] `update_checker.h/.cpp` - New module for version checking
- [x] `web_server.cpp` - State broadcast, WebSocket commands, recovery UI
- [x] `animatronic-eyes.ino` - Include and init updateChecker
- [x] `data/index.html` - Header indicator, Update Check config card
- [x] `data/style.css` - Update indicator and status styles
- [x] `data/app.js` - Indicator logic, handlers, UI updates

---

## ✅ v0.9 - Admin Lock

Server-side authentication to protect configuration and calibration from unauthorized changes.

### Deferred from v0.8

- [x] Refactor mode.available - Send available modes/impulses on connect, not in every state broadcast

### Protection Model

- [x] Admin PIN (4-6 digits) stored in NVS
- [x] PIN required for sensitive operations on STA connections
- [x] AP connections auto-unlocked (AP password = implicit auth)
- [x] No PIN set = always unlocked (default state)
- [x] Recovery UI factory reset always allowed (escape hatch)

### Protected Actions (require unlock)

- [x] WiFi credentials (save/forget)
- [x] AP settings
- [x] Calibration changes
- [x] LED settings
- [x] mDNS settings
- [x] OTA firmware upload
- [x] OTA UI upload
- [x] Reboot (special: allowed when locked, blocked when rate limited)

### Always Allowed

- [x] Control tab (gaze, lids, blink, center)
- [x] View any config (read-only when locked)
- [x] Recovery UI factory reset

### Unlock Conditions

| Context | Behavior |
|---------|----------|
| Connected via AP (192.168.4.x) | Auto-unlocked, shows wrench icon (AP) |
| Connected via STA, no PIN set | Auto-unlocked, shows wrench icon |
| Connected via STA, PIN set | Locked, enter PIN to unlock |
| Unlocked via PIN | 15 minute timeout, shows countdown |

### Firmware Implementation

- [x] Store PIN in NVS (plain text, physical access = game over anyway)
- [x] Detect AP vs STA client by IP subnet (192.168.4.x = AP)
- [x] IP-based auth state (simpler than tokens, survives page reload)
- [x] Auth state validated on protected WebSocket commands
- [x] Auth expires after 15 minutes
- [x] Rate limit: 3 failed attempts → 5 minute lockout

### WebSocket Commands

```json
{"type": "adminAuth", "pin": "1234"}
{"type": "adminLock"}
{"type": "setAdminPin", "pin": "1234"}
{"type": "clearAdminPin"}
```

### Admin State Message (separate from state broadcast)

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

### HTTP Endpoints (for Recovery UI)

- [x] `GET /api/admin-status` - Check lock state
- [x] `POST /api/unlock` - Unlock with PIN

### UI - Connection Bar

```
Locked:      Connected - 192.168.1.50 🔒
Unlocked:    Connected - 192.168.1.50 🛠️ 14:32
AP bypass:   Connected - 192.168.4.2 🛠️ (AP)
No PIN set:  Connected - 192.168.1.50 🛠️
```

- [x] 🔒 icon when locked (tappable → PIN modal)
- [x] 🛠️ icon when unlocked (tappable → lock)
- [x] Countdown timer (mm:ss) when unlocked via PIN
- [x] Green background when connected, yellow when unlocked

### UI - Tabs

- [x] `🔒 Calibration` and `🔒 Configuration` when locked
- [x] No icon when unlocked
- [x] Control and Console tabs never show lock

### UI - Locked Tab Behavior

- [x] Tab opens (can view all content)
- [x] Edit controls disabled (grayed out, non-functional)

### UI - PIN Modal

- [x] 4-6 digit numeric keypad
- [x] Mobile-friendly large buttons
- [x] Backspace and clear buttons
- [x] Cancel button
- [x] Error feedback on wrong PIN
- [x] Lockout display with countdown when rate limited

### UI - PIN Management (in Config, when unlocked)

- [x] "Set PIN" / "Clear PIN" buttons
- [x] Shows current state (PIN configured / not configured)

### Recovery UI Integration

- [x] PIN entry in lock banner
- [x] Protected actions disabled when locked
- [x] Reboot allowed when locked, blocked when rate limited

### Recovery Path if PIN Forgotten

1. Connect to device AP (requires AP password) → auto-unlocked
2. Or: Access `/recovery` → factory reset → clears PIN

### Additional Changes

- [x] WebSocket broadcast interval reduced to 75ms for smoother blink animations
- [x] System cards consistent styling in Configuration tab

### File Changes

- [x] `config.h` - Admin timeout constants (900s unlock, 300s lockout)
- [x] `storage.h/.cpp` - PIN storage in NVS
- [x] `web_server.h/.cpp` - Auth commands, IP-based auth, AP client detection
- [x] `web_server.cpp` - Protected command checks, HTTP endpoints
- [x] `data/index.html` - Lock icons, PIN management UI
- [x] `data/style.css` - Modal styles, lock icon styles, disabled state styles
- [x] `data/app.js` - Auth flow, PIN modal, countdown timer, UI lock state

---

## ✅ v0.8 - Impulses

One-shot animations that interrupt current state, then restore it. Works in both Follow and Auto modes.

### Features

- [x] Impulse JSON format - Like modes but with `restore: true` instead of `loop: true`
- [x] State save/restore - Gaze X/Y/Z, coupling, lids saved before impulse, restored after
- [x] Preload system - Next random impulse preloaded for instant trigger
- [x] Auto-impulse - Periodic random impulses (works in all modes like auto-blink)
- [x] Impulse selection - Checkboxes to select subset for auto/random
- [x] Control tab buttons - `[Link] [Impulse] [Auto-I] [Blink] [Auto-B]`
- [x] Precedence - Impulse waits for blink to finish, blink skipped during impulse
- [x] Backup/restore - Impulse files included (same pattern as modes)
- [x] OTA safety - Stops all servo activity during firmware/UI uploads
- [x] Console logging for auto-blink, manual blink/wink, auto-impulse, impulse playback

### JSON Format

```json
{
  "name": "Startle",
  "restore": true,
  "sequence": [
    {"lids": {"left": 100, "right": 100}},
    {"gaze": {"x": {"random": [-30, 30]}, "y": {"random": [10, 30]}}},
    {"wait": 200}
  ]
}
```

Same primitives as modes (gaze, lids, blink, wait). `restore: true` triggers state save/restore.

### Storage

```
/impulses/
  startle.json
  distraction.json
```

### Config Additions

```json
{
  "autoImpulse": true,
  "impulseIntervalMin": 30000,
  "impulseIntervalMax": 120000,
  "impulseSelection": ["startle", "distraction"]
}
```

### UI - Control Tab

```
[Link] [Impulse] [Auto-I] [Blink] [Auto-B]
```

- Impulse button triggers random from selected subset (preloaded)
- Auto-I toggle enables/disables auto-impulse
- Both disabled when selection empty
- Auto-I disabled in auto modes (like Auto-B)

### UI - Mode Settings (Config Tab)

```
├── Auto-Impulse [checkbox]
├── Impulse Interval [min] to [max]
└── Impulse Selection
    [✓] Startle
    [✓] Distraction
    [ ] custom-impulse
```

### Preload System

- On startup: preload random impulse from selection
- On trigger: play preloaded, then preload next
- If preloaded no longer in selection: still play, next preload from current list
- Memory: ~500 bytes for preloaded sequence

### Precedence Rules

| Situation | Behavior |
|-----------|----------|
| Impulse during blink | Wait for blink to finish, then play |
| Blink during impulse | Skip blink |
| Auto-blink timer during impulse | Skip, reset timer after |

### WebSocket Changes

- Remove `mode.available` from state broadcast
- Add `getAvailableModes` / `getAvailableImpulses` (request on connect)
- Add `triggerImpulse`, `setAutoImpulse`, `setImpulseInterval`, `setImpulseSelection`

### Default Impulses

**Startle** - Wide eyes + random jerk movement, 200ms

**Distraction** - Quick random side glance, 150-300ms

### Defaults

- Auto-Impulse: on
- Interval: 30-120 seconds
- Selection: startle, distraction (built-ins)

### Implementation Order

1. Basic impulse trigger (manual, no preload, no auto)
2. Add preload system
3. Add auto-impulse
4. Add selection checkboxes in config

### Memory Budget (~530 bytes)

| Item | Size |
|------|------|
| Preloaded sequence | ~500 bytes |
| Saved state | ~28 bytes |
| Pending flag | 1 byte |

---

## ✅ v0.7 - Mode System

JSON-defined autonomous behaviors. **XOR with Follow Mode** - one active at a time.

### Primitives Engine

| Primitive | Parameters | Description |
|-----------|------------|-------------|
| `gaze(x, y, z)` | -100 to +100 each | Look at point |
| `lids(left, right)` | -100 to +100 | Set eyelid positions |
| `blink(duration)` | milliseconds (0 = auto-scale) | Quick blink |
| `wait(ms)` | milliseconds | Pause |
| `random(min, max)` | range | Randomness for generative behaviors |

### Mode Infrastructure

- [x] JSON mode definitions stored on LittleFS (`/modes/`)
- [x] Mode loader/player with loop support
- [x] Coupling parameter per mode (negative for Feldman/divergent)
- [x] Auto-blink system (works in Follow and Auto modes)
- [x] Backup/restore includes mode files

### Built-in Modes

| Mode | Behavior |
|------|----------|
| **Natural** | Random looking, occasional blinks |
| **Sleepy** | Slow movements, heavy lids, long blinks |
| **Alert** | Wide eyes, quick darting movements |
| **Spy** | Narrowed lids, suspicious left-right shifting, occasional "normal" look |
| **Crazy** | Independent eyes (Feldman mode), erratic movements |

### UI Changes

- [x] Mode dropdown selector on Control tab (Follow + Auto modes)
- [x] Mode settings section (default mode, auto-blink config)
- [x] Manual control pauses mode, then resumes

### JSON Format Example

```json
{
  "name": "Natural",
  "loop": true,
  "coupling": 1.0,
  "sequence": [
    {"gaze": {"x": 0, "y": 0, "z": 50}},
    {"wait": 2000},
    {"gaze": {"x": {"random": [-30, 30]}, "y": {"random": [-20, 20]}}},
    {"wait": {"random": [1000, 3000]}},
    {"blink": 150},
    {"wait": {"random": [3000, 8000]}}
  ]
}
```

---

## ✅ v0.6 - Follow Mode

Direct pointer/touch control of gaze. **XOR with Mode System** - one active at a time.

### Features

- [x] Touch/mouse position → gaze X/Y mapping (gaze pad)
- [x] Z control via slider (scroll wheel handled by browser on focused slider)
- [x] "Tap on nose" effect (vergence - eyes converge when Z is close)
- [x] Follow area widget on Control tab (gaze pad)

### Design Decisions

- **No auto-return to center**: Eyes stay where positioned. Center button available for explicit reset.
- **No custom scroll/pinch handlers**: Browser natively sends scroll events to focused sliders.

### Implementation Notes

Follow Mode maps screen coordinates to gaze coordinates. The gaze pad is a defined region on the Control tab where touch/mouse position is tracked and translated to eye movement via the Eye Controller abstraction.
