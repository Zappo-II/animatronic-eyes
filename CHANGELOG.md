# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Changed
- **README Quick Start** - Added multiple flash options (web flasher, Arduino IDE, command line with esptool hints)

---

## [1.0.0] - 2026-01-04

### Added
- **Update Check** - Automatic check for newer firmware versions on GitHub
- Configurable frequency: boot only, daily, or weekly (default: daily)
- Manual "Check Now" button in System section
- Yellow pulsing indicator in header when update available (clicks to GitHub releases)
- Update status displayed in System config card
- Cached result persisted to NVS (survives reboot)
- Cache auto-clears when user updates firmware
- Recovery UI shows cached update status
- WebSocket commands: `checkForUpdate`, `setUpdateCheckEnabled`, `setUpdateCheckInterval`
- State broadcast includes `update` object: `available`, `version`, `lastCheck`, `checking`, `enabled`, `interval`

### Changed
- **WebSocket broadcast interval** - Increased from 75ms to 100ms to reduce connection drops
- **Vergence intensity** - Reduced max vergence from 100 to 50 for less aggressive eye convergence

### Fixed
- **Impulse log order** - "Impulse triggered" now logged before "pending/playing" (was reversed)
- **Invert checkbox safety** - Toggling invert now immediately moves servo to center position instead of mirroring current position, preventing potential mechanical damage when servo is near an extreme
- **Invert immediate feedback** - Invert checkbox change is now sent to device immediately, so user can see the effect on test slider right away
- **Blink not closing from wide open** - Blink duration now scales based on lid travel distance; lids at +100 get ~250ms, neutral gets ~175ms. Use `{"blink": 0}` in modes/impulses for auto-scaling, or specify explicit duration
- **Connection status stuck on "Connected"** - Web UI now detects dead connections via heartbeat; shows "Disconnected" within 3 seconds if device is powered off

### Documentation
- **Calibration guide** - Reordered steps: safe test range → direction → min → max → center (safer procedure)
- Custom modes and impulses highlighted in README feature list with doc links
- Added "Custom Modes" and "Custom Impulses" sections to usage.md with JSON format and examples

### Technical
- New `UpdateChecker` singleton class (`update_checker.h/.cpp`)
- New NVS storage: `UpdateCheckConfig`, `UpdateCheckCache` structs
- HTTPS fetch using `WiFiClientSecure` with `setInsecure()` for GitHub CDN
- Semver comparison: major.minor.patch
- 30s boot delay + random jitter (0-30 min) to prevent thundering herd
- Check only when WiFi STA connected (skipped in AP-only mode)

---

## [0.9.17] - 2026-01-03

### Added
- **README badges** - Release version, license, and download count badges

### Changed
- **Yellow indicator colors** - Unlocked/warning states now use true yellow instead of brownish tint
- **Control tab layout** - Link toggle left-aligned; buttons reordered to Impulse, Auto-I, Blink, Auto-B (right-aligned)
- **README Quick Start** - Updated to use pre-built binaries and recovery UI for initial setup

### Fixed
- **Auto-Impulse toggle** - Now uses runtime override like Auto-Blink (doesn't persist to config)
- **PIN timeout UI** - Connection bar color now updates immediately when timeout expires
- **Recovery UI buttons** - Action buttons now have proper spacing when wrapping on mobile

---

## [0.9.16] - 2026-01-02

### Added
- **Admin Lock** - PIN-based protection for configuration changes
- 4-6 digit PIN configured via Configuration tab
- AP-connected clients (192.168.4.x) always unlocked - no PIN required
- 15-minute unlock timeout with countdown displayed in connection status bar
- Rate limiting: 3 failed PIN attempts → 5-minute lockout
- IP-based authentication - unlocks persist across page reloads and work across multiple tabs
- PIN modal with keypad for unlock, shows "Incorrect PIN" feedback and lockout countdown
- Recovery UI PIN entry - unlock directly from `/recovery` page after firmware updates
- Reboot allowed when locked, blocked only when rate limited (prevents lockout bypass)
- Lock/unlock icons in connection status bar and on protected tabs (Calibration, Configuration)
- WebSocket commands: `adminAuth`, `adminLock`, `setAdminPin`, `clearAdminPin`, `getAdminState`
- HTTP endpoints: `/api/admin-status`, `/api/unlock`

### Changed
- **WebSocket broadcast interval** reduced from 200ms to 75ms for smoother blink animations in UI
- System cards in Configuration tab now use consistent styling (`system-card` class)
- Protected controls disabled when locked (inputs, buttons, sliders in Calibration and Configuration tabs)

### Fixed
- PIN modal now waits for server response before closing (shows error on failure)
- Auth state properly persists across page reloads (IP-based, not WebSocket client ID)
- Rate limit log messages now appear in correct order (cause → consequence)

### Technical
- `ClientAuthState` struct simplified - removed `clientId`, now fully IP-based
- `authenticateClient()`, `isClientAuthenticated()`, `getRemainingSeconds()` all use IP lookup
- `removeClientAuth()` removed (auth entries kept on WebSocket disconnect for page reload support)
- `broadcastAdminStateToIP()` notifies all connected tabs from same IP on lock/unlock
- Rate limit entries stored in RAM (cleared on reboot)
- Server-side protection on `/api/reboot` and WebSocket `reboot` command when rate limited

---

## [0.9.0] - 2026-01-02

### Changed
- **WebSocket optimization** - Available modes/impulses now sent once on connect instead of in every state broadcast
- Reduces broadcast payload by ~80 bytes per message (5 messages/sec = 400 bytes/sec saved)

### Technical
- New `sendAvailableLists()` method sends `availableModes` and `availableImpulses` messages on WebSocket connect
- Removed `mode.available` and `impulse.available` arrays from state broadcast
- Added `availableImpulsesList` variable in app.js to store impulses for config form population
- Added handlers for `availableModes` and `availableImpulses` message types

---

## [0.8.7] - 2026-01-02

### Added
- **Impulse System** - One-shot animations that interrupt current state, then restore it
- Built-in impulses: Startle (wide eyes + jerk movement), Distraction (quick side glance)
- Impulse JSON format with `restore: true` flag (same primitives as modes)
- State save/restore - Gaze X/Y/Z, coupling, lids saved before impulse, restored after
- Preload system - Next random impulse preloaded for instant trigger response
- Auto-impulse - Periodic random impulses (configurable interval, like auto-blink)
- Impulse selection - Checkboxes in Config tab to select subset for auto/random
- Control tab buttons: `[Auto-I] [Impulse]` alongside existing `[Link] [Blink] [Auto-B]`
- Precedence rules: Impulse waits for blink to finish, blink skipped during impulse
- Backup/restore includes impulse files (same pattern as modes)
- Impulse state in WebSocket broadcast (playing, pending, preloaded, selection)
- Impulse Settings section in Configuration tab
- WebSocket commands: `triggerImpulse`, `setAutoImpulse`, `setImpulseInterval`, `setImpulseSelection`, `getAvailableImpulses`
- OTA safety - Stops all servo activity during firmware and UI uploads
- Console logging for auto-blink, manual blink/wink, auto-impulse, and impulse playback
- Device ID in `/api/version` response for About section display

### Fixed
- Auto-blink config not loading from storage on startup (was always enabled)
- Auto-impulse disabled after mode switch (now works in all modes like auto-blink)
- Log ordering - "Auto-triggered" now appears before "Playing" in console
- About section deviceId display (no more trailing dash when missing)

### Changed
- Impulse interval defaults changed to 15-25 seconds (was 30-120)
- Auto-impulse now uses runtime override pattern (matches auto-blink behavior)

### Technical
- New classes: ImpulsePlayer, AutoImpulse
- Impulse JSON files stored in LittleFS `/impulses/` directory
- ImpulseConfig added to Storage for persistent settings
- Public `stop()` method added to ImpulsePlayer for OTA safety

---

## [0.7.16] - 2026-01-01

### Added
- **Remember last active mode** - Checkbox in Mode Settings that auto-saves current mode as startup mode on every mode switch
- Startup mode dropdown disabled when "Remember" is checked (read-only, auto-updates)
- Safe state (Mode::NONE) during firmware upload, UI upload, and backup restore to prevent brownouts
- Reboot signaling before restart after uploads/restore

### Fixed
- LittleFS mount order - now mounted before mode system initialization (modes were failing to load on startup)
- Disabled dropdown now visually distinct (dimmed, no hover effect)

### Changed
- Blink interval form fields now display on single line

## [0.7.9] - 2026-01-01

### Changed
- Static JsonDocument for WebSocket message parsing (reduces stack pressure)
- ServoConfig getConfig() now returns const reference (reduces stack usage)

## [0.7.8] - 2026-01-01

### Fixed
- Slider activeElement checks removed - sliders now update correctly after mode overrides them

## [0.7.7] - 2026-01-01

### Fixed
- Mode selector jitter - added 1 second lock after user selection to prevent state broadcast from reverting

## [0.7.6] - 2026-01-01

### Fixed
- Mode switching now fully resets eye state including Z and coupling via new `resetAll()` method
- Mode player `start()` now clears pause state from previous manual control
- Animation state cleared on mode switch to prevent interference

### Changed
- Removed `<hr>` after system-actions in Config tab
- Removed blink debug logging

## [0.7.5] - 2026-01-01

### Fixed
- **Critical**: Blink/wink buttons now work reliably - switched from blocking to async blink functions
- Servo paired writes - eyelids now always written together for synchronized blink
- Servo throttling rewritten to prioritize paired movements (eyelids, eye X, eye Y)

## [0.7.4] - 2026-01-01

### Fixed
- **Critical**: Servo control completely broken since v0.6 - Arduino's `map()` only works with integers, added `mapFloat()` helper

## [0.7.3] - 2026-01-01

### Fixed
- Auto-blink alternating wink issue - mode player `execLids()` now skips during animation
- Backup/restore section double horizontal line removed

## [0.7.2] - 2026-01-01

### Fixed
- Manual blink/wink now resets auto-blink timer (prevents double-blink)
- Manual lid control resets auto-blink timer
- Mode player pause/resume during manual control interactions
- Increased control lock from 150ms to 300ms

## [0.7.1] - 2026-01-01

### Fixed
- Gaze pad jumpiness - added control locking mechanism
- Blink double-triggering - added blinkAnimationActive flag
- Lid controls synchronization with WebSocket state updates

## [0.7.0] - 2025-12-31

### Added
- **Mode System** - Autonomous eye behaviors with JSON-defined sequences
- 5 built-in modes: Natural, Sleepy, Alert, Spy, Crazy
- Mode primitives: gaze, lids, blink, wait with random value support
- Coupling parameter for modes (negative = Feldman/divergent mode)
- Mode selector dropdown in Control tab (Follow + Auto modes)
- Auto-blink system works in both Follow and Auto modes
- Configurable blink interval (2-6 seconds default)
- Default startup mode setting
- **Backup/Restore** - Complete device configuration backup
- Download backup (JSON) in main UI and recovery UI
- Restore backup from main UI
- Includes: calibration, WiFi settings, mode settings, mode files

### Changed
- Eye controller now uses async (non-blocking) blink and wait animations
- Mode state broadcast in WebSocket state messages
- Recovery UI now includes "Download Backup" button

### Technical
- New classes: AutoBlink, ModeManager, ModePlayer
- Async animation state machine in EyeController
- Mode JSON files stored in LittleFS /modes/ directory
- ModeConfig added to Storage for persistent settings

---

## [0.6.0] - 2025-12-31

### Added
- **Follow Mode** - Touch/mouse gaze control via gaze pad
- Gaze pad widget for intuitive X/Y eye control
- Z depth slider for vergence control
- Vergence effect (eyes converge when looking close)
- Coupling control (-1 divergent to +1 linked)
- Copyright notice in web UI About section
- Firmware upload hints in UI (which .bin file to use)

### Changed
- Documentation fully restructured for GitHub publication
- All source files now have copyright headers
- LICENSE clarified for commercial bundling scenarios
- Protective code comments added for critical bug fixes

## [0.5.17] - 2025-12-30

### Changed
- Documentation restructured for GitHub publication

## [0.5.16] - 2025-12-29

### Added
- About section in Configuration tab with version/device info
- Header title links to GitHub repository
- Info icon in header toggles About section
- Device chip info (model, revision) from ESP32 API
- LICENSE file (CC BY-NC-SA 4.0)
- Credits for Morgan Manly with links to GitHub, Instructables, Makerworld
- Commercial usage warning with styled alert

## [0.5.15] - 2025-12-29

### Added
- About section initial implementation

### Changed
- Info icon behavior - toggles instead of just opening

## [0.5.14] - 2025-12-29

### Changed
- Default coupling to 1 (linked with vergence, was 0)
- Default gazeZ to 0 (medium depth, was 100)
- Center button no longer resets Z or coupling (preserves user settings)

## [0.5.13] - 2025-12-29

### Fixed
- Blink animation now uses JS Web Animations API (animates from current position)
- Vergence simulation used `||` which treated 0 as falsy, now uses `??`
- JS simulation defaults now match firmware

### Added
- Vertical divergence when coupling < 0 (Feldman mode)

## [0.5.12] - 2025-12-29

### Fixed
- Calibration tab mobile layout - center slider on own row
- Eye preview eyelids now circular (were rectangular when closed)
- Slider tooltip positioning in lid controls

### Changed
- Control tab layout unified for desktop and mobile
- Reordered Control tab: Eye preview, Gaze pad, Lid controls, Center, Depth/Coupling
- Combined Depth and Coupling sliders into shared card
- Combined lid controls into single card with shared header

### Added
- Scroll safe zones on mobile for all control and calibration sliders

## [0.5.11] - 2025-12-28

### Changed
- Default coupling now 0 (independent)
- Individual lid buttons renamed "Blink" to "Wink"

### Fixed
- Blink animation improved (smoother timing)
- Console and Configuration tabs now have consistent headers
- Upload progress shows "Processing..." after 100%

## [0.5.10] - 2025-12-28

### Changed
- Link icon enlarged for better visibility
- Link tooltip now dynamic based on state
- Global Blink button moved under link icon

## [0.5.9] - 2025-12-28

### Added
- Lid link toggle - chain icon to control both lids together
- Link enabled by default

## [0.5.8] - 2025-12-28

### Added
- Single-eye blink buttons (Wink)
- `blinkLeft()` / `blinkRight()` methods and WebSocket commands

### Changed
- Control tab layout restructured
- X/Y gaze values moved above gaze pad

## [0.5.7] - 2025-12-28

### Changed
- Max vergence increased from 80 to 100 (full cross-eye range)
- Center button now also resets coupling to 0

## [0.5.6] - 2025-12-28

### Changed
- Control tab slider labels/values moved above sliders (touch-friendly)
- Max vergence increased from 30 to 80

### Fixed
- Coupling slider now has drag tooltip

## [0.5.5] - 2025-12-28

### Fixed
- Eyelids now close completely in preview
- Blink button now shows animation in eye preview

## [0.5.4] - 2025-12-28

### Added
- Live eye preview at top of Control tab
- Vergence visualization (pupils converge when Z is close)
- Two-part eyelids (top and bottom) that meet in center

## [0.5.3] - 2025-12-28

### Fixed
- Test sliders now continuously sync to actual servo positions
- Center All button in Calibration tab now updates test sliders

## [0.5.2] - 2025-12-28

### Added
- Eye Controller state re-applied when returning to Control tab from Calibration
- Toast notification "Resuming previous pose"
- `reapply()` method and `reapplyEyeState` WebSocket command

## [0.5.1] - 2025-12-28

### Added
- Center All button in Calibration tab header
- Test sliders sync to actual servo positions when switching tabs

### Changed
- Center button sets lids to calibration center instead of fully open
- Initial lid position on startup changed to calibration center

## [0.5.0] - 2025-12-28

### Added
- Eye Controller abstraction layer (`eye_controller.h/.cpp`)
- Gaze control with X/Y touch pad and Z depth slider (-100 to +100)
- Automatic vergence (eye convergence based on Z depth)
- Coupling parameter (-1 divergent to +1 linked)
- Eyelid control with left/right sliders (-100 closed to +100 open)
- Blink button for instant blink
- [Follow | Auto] mode toggle (Auto placeholder)
- Test slider in each Calibration card
- WebSocket commands: setGaze, setLids, blink, setCoupling, setVergence, centerEyes

### Changed
- Control tab completely redesigned with gaze pad

## [0.4.16] - 2025-12-28

### Fixed
- Calibration center slider tooltip position

## [0.4.15] - 2025-12-28

### Added
- Drag tooltip for LED brightness slider

## [0.4.14] - 2025-12-28

### Added
- Drag tooltips for all servo control sliders

## [0.4.13] - 2025-12-28

### Changed
- Calibration center value display moved above slider

## [0.4.12] - 2025-12-28

### Added
- Drag tooltip for calibration center slider

## [0.4.11] - 2025-12-28

### Fixed
- Calibration preview now bypasses stored calibration limits
- Calibration save no longer causes servo jitter
- Slider responsiveness improved with leading+trailing throttle

### Added
- `setPositionRaw()` method for calibration preview without clamping

### Changed
- Servo updates now prioritize servos with pending changes

## [0.4.10] - 2025-12-27

### Fixed
- Servo 0 (Left Eye X) not responding due to LEDC timer conflict

### Changed
- Initialization order - servos now initialize before LED

## [0.4.9] - 2025-12-27

### Added
- LED brightness control (1-255) via PWM dimming
- Brightness slider in Configuration tab

### Changed
- LED output converted from digital to 8-bit PWM (5kHz)

## [0.4.8] - 2025-12-27

### Added
- Visibility change handler for immediate reconnect when tab becomes active

## [0.4.7] - 2025-12-27

### Changed
- WiFi Primary section now starts collapsed by default

## [0.4.6] - 2025-12-27

### Changed
- Main UI System Info now matches Recovery UI format
- Removed "v" prefix from version displays

## [0.4.5] - 2025-12-27

### Fixed
- Servo attach error check (channel 0 is valid)

## [0.4.4] - 2025-12-27

### Added
- Console tab with web log viewer
- Ring buffer (50 lines) for log history
- Real-time log streaming via WebSocket
- Color-coded log lines by tag
- Auto-scroll toggle and clear button
- `WEB_LOG()` macro for dual Serial+WebSocket logging
- Bidirectional version checking
- Automatic redirect to recovery for version failures

## [0.4.3] - 2025-12-26

### Added
- Granular UI version status: ok, minor_mismatch, major_mismatch, incompatible, missing
- minFirmware check from version.json

### Changed
- API returns `uiStatus` string instead of booleans

## [0.4.2] - 2025-12-26

### Added
- Tap-to-show tooltips on mobile
- Loading spinner + toast feedback for buttons
- "Unconfigured" indicator on WiFi sections
- Selection-aware WiFi Status updates
- Right margin on Control tab sliders for mobile scroll

### Changed
- Device name and ID combined in header
- Recovery UI buttons moved into System section

## [0.4.1] - 2025-12-24

### Added
- Device ID display in page header
- mDNS hostname always appends device ID
- Eye button to show/hide AP password
- Detailed tooltips on header status indicators
- WiFi Status section with Name/IP grid layout
- Connected indicator in WiFi section headers
- On/Off badge in AP, mDNS, LED section headers
- Dirty state tracking for calibration cards
- Global config footer with Reboot, Recovery UI, Wipe UI, Factory Reset

### Changed
- Calibration card order: L X,Y, R X,Y, L Lid, R Lid

## [0.4.0] - 2025-12-23

### Fixed
- Configuration fields snap-back bug

### Added
- On-demand config fetching via `getConfig` WebSocket command
- Collapsible configuration sections
- Per-section Save/Revert buttons with dirty state tracking
- Visual dirty indicator on modified sections
- Global "Reload Config" button
- Toast notifications for save feedback

### Changed
- Config data removed from state broadcast
- WiFi Primary/Secondary as separate sections
- "Keep AP on" moved to AP Settings section

## [0.3.1] - 2025-12-22

### Added
- Tab-based UI navigation (Control | Calibration | Configuration)
- On-demand WiFi network scanning with dropdown
- Lock icons for secured networks
- Password visibility toggle
- AP configuration (SSID prefix and password)
- Reboot required indicator
- Header status indicators (AP, WiFi, UI, Reboot)
- Recovery mode with embedded UI at `/recovery`
- "Wipe UI Files" and "Factory Reset" in recovery mode
- LittleFS filesystem upload for UI updates
- UI version tracking and compatibility checking
- Live servo preview during calibration

### Changed
- Calibration UI redesigned with +/- buttons, center slider
- Default servo calibration values to 89/90/91
- Network configuration uses "Connect" and "Forget" buttons
- Clamping rules: min <= center <= max

## [0.3.0] - 2025-12-22

### Added
- Multi-SSID support (primary + secondary network)
- WiFi reconnection logic with grace period and retries
- mDNS hostname support (device as `hostname.local`)
- LED status indicator with blink patterns
- Configurable WiFi timing via Web UI
- "Keep AP active when connected" toggle

### Changed
- UI reorganized into Servo Control and Device Settings sections
- Servo calibration moved to table format
- WiFi networks stored with index (wifi0/wifi1)

## [0.2.16] - 2025-12-09

### Fixed
- Task Watchdog Timer crash when using centerAll or moving multiple servos
- WiFi mode not being reset before reconnection attempts

### Added
- NVS write verification for WiFi credentials
- Robust factory reset with verification
- Servo write throttling (20ms interval)
- Deferred broadcast from WebSocket handler

### Changed
- Static JSON buffer in broadcastState() to reduce stack usage

## [0.2.0] - 2025-12-01

### Added
- Initial stable minimal core
- WiFi AP mode on first boot
- WiFi STA mode with stored credentials
- Fallback to AP if network unavailable
- Web server on port 80
- LittleFS for web assets
- WebSocket real-time communication
- 6 servo sliders with live control
- Per-servo calibration (min/center/max)
- Per-servo invert flag
- Per-servo pin configuration
- Calibration validation (0-180, min < center < max)
- Slider throttling (50ms)
- Focus-aware input updates
- Center All button
- WiFi configuration via web UI
- Leave Network function
- OTA firmware upload
- Reboot via web UI
- Factory Reset via web UI
- Settings persist across reboots (NVS)

