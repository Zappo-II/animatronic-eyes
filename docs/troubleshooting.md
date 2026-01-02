# Troubleshooting & FAQ

Common problems and their solutions.

## Frequently Asked Questions

### Can I use a different ESP32 board?

Yes, any ESP32 with sufficient GPIO pins should work. You may need to adjust the default pin assignments in the web UI.

### Can I use different servos?

Yes, any PWM servo compatible with the ESP32Servo library will work. MG90S are recommended for their size and torque. Larger servos may require external power.

### Why is my device called "LookIntoMyEyes-XXXXXX"?

The 6 characters are derived from your ESP32's unique chip ID. This ensures each device has a unique name on the network.

### Can I change the AP password?

Yes, in Configuration → AP Settings. Changes require a reboot.

### How do I find my device's IP address?

1. Check Serial Monitor output on boot
2. Use mDNS: `animatronic-eyes-XXXXXX.local`
3. Check your router's connected devices list

### Does this work without WiFi?

Yes! The device creates its own Access Point. You only need WiFi if you want to connect through your home network.

---

## Known Issues

| Issue | Status | Workaround |
|-------|--------|------------|
| USB power insufficient for all servos | Hardware | Use external 5V 2A supply |

---

## Connection Problems

### WiFi network doesn't appear

**Symptoms**: Can't find "LookIntoMyEyes-XXXXXX" in WiFi list

**Solutions**:
1. Wait 30 seconds after powering on
2. Check Serial Monitor for errors
3. Ensure ESP32 is powered properly
4. Try resetting the ESP32

### Can't connect to AP

**Symptoms**: WiFi password rejected

**Solutions**:
1. Default password is `12345678`
2. If you changed it and forgot, use recovery mode to factory reset

### Web page won't load

**Symptoms**: Browser shows error or blank page

**Solutions**:
1. Confirm you're connected to the device's WiFi (not your home network)
2. Try http://192.168.4.1 directly
3. Check if LittleFS was uploaded (see [Setup Guide](setup.md#step-7-upload-web-ui-files))
4. Access recovery mode: http://192.168.4.1/recovery

### WebSocket keeps disconnecting

**Symptoms**: "Connecting..." appears frequently, controls stop responding

**Solutions**:
1. Move closer to the device
2. Check for WiFi interference
3. Close other browser tabs to the same device
4. Check free heap in System section (memory issues if very low)

### Can't connect to home WiFi

**Symptoms**: Device falls back to AP mode after trying to connect

**Solutions**:
1. Verify SSID and password are correct
2. Ensure your router is 2.4GHz (ESP32 doesn't support 5GHz)
3. Check if router is blocking new devices
4. Try the secondary network slot
5. Increase retry attempts in WiFi Timing settings

---

## Servo Problems

### Servo doesn't move

**Solutions**:
1. Check wiring (signal, power, ground)
2. Verify GPIO pin assignment in Calibration tab
3. Check if servo is getting power (USB may not supply enough current)
4. Try the test slider in Calibration tab
5. Check Serial Monitor for attach errors

### Servo jitters or vibrates

**Causes**:
- Mechanical binding at position limits
- Insufficient power supply
- Calibration range exceeds physical limits

**Solutions**:
1. Reduce Min/Max range in calibration
2. Use a proper 5V 2A+ power supply
3. Check for mechanical obstructions

### Device reboots randomly during use

**Symptoms**: Device resets unexpectedly, especially when servos are active. Serial output shows `E BOD: Brownout detector was triggered`.

**Cause**: Insufficient power supply. USB ports typically provide ~500mA, but ESP32 + 6 servos can draw 1.5-2A during movement.

### Power Configuration Options

#### Option 1: USB Only (Development/Testing)

- USB cable to ESP32
- ESP32 5V pin → servo rail V+
- ESP32 GND pin → servo rail GND
- **Do NOT connect external power to servo rail**

| Pros | Cons |
|------|------|
| Serial console available | Likely to brownout under load |
| Simple setup | Limited to ~500mA |

**Tip**: Disconnect servos to prevent brownout during development/debugging.

#### Option 2: External Power Only (Standalone)

- 5V 2A+ supply → servo rail V+ and GND
- Servo rail V+ → ESP32 5V pin
- Servo rail GND → ESP32 GND pin
- No USB connected

| Pros | Cons |
|------|------|
| Sufficient power (2A+) | No serial console |
| No brownouts | |

#### Option 3: Both (Recommended)

- USB cable to ESP32 (serial/programming only)
- 5V 2A+ supply → servo rail V+ and GND
- ESP32 GND → servo rail GND (**common ground only**)
- **Do NOT connect ESP32 5V to servo rail V+**

| Pros | Cons |
|------|------|
| Serial console available | Slightly more complex wiring |
| Sufficient power (2A+) | |
| No brownouts | |

**Warning**: Never connect two 5V sources together (USB 5V and external 5V). Only share ground between ESP32 and servo rail when using both power sources.

---

## Update Problems

### OTA firmware upload fails

**Solutions**:
1. Ensure .bin file is correct format
2. Check available flash space
3. Use a stable WiFi connection
4. Try recovery mode if main UI is broken

### UI upload fails

**Solutions**:
1. Ensure ui.bin was created with correct mklittlefs version
2. Check file size (should be ~1.4MB for default partition)
3. Use recovery mode for UI upload

### Creating ui.bin manually

If you need to create the LittleFS image manually (e.g., for OTA upload):

#### Finding mklittlefs

The tool may already be installed:

1. **System-wide** (if installed via package manager):
   ```bash
   which mklittlefs
   # /usr/bin/mklittlefs
   ```

2. **Arduino ESP32 tools** (installed with ESP32 board support):
   ```bash
   ls ~/.arduino15/packages/esp32/tools/mklittlefs/*/mklittlefs
   ```

#### Installing mklittlefs

| Platform | Method |
|----------|--------|
| Arch Linux | `yay -S mklittlefs-bin` (AUR) |
| ESP32 Arduino | Included with board support package |
| Other | Build from source: https://github.com/earlephilhower/mklittlefs |

#### Creating the image

```bash
mklittlefs -c data/ -p 256 -b 4096 -s 0x160000 ui.bin
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `-c data/` | | Source directory containing web files |
| `-p 256` | 256 bytes | Page size (ESP32 standard) |
| `-b 4096` | 4096 bytes | Block size (ESP32 standard) |
| `-s 0x160000` | 1.44 MB | Partition size (matches "Default 4MB with spiffs") |
| `ui.bin` | | Output filename |

---

### Version mismatch warning

**Symptoms**: Yellow or red UI indicator in header

**Solutions**:
1. Yellow (minor mismatch): Update recommended but not required
2. Red (major mismatch): Update strongly recommended
3. Upload matching firmware and UI versions
4. Check CHANGELOG for compatibility requirements

---

## Mode System Problems

### Auto mode doesn't start

**Symptoms**: Select an auto mode but eyes don't move

**Solutions**:
1. Check Console tab for mode loading errors
2. Verify mode files exist in `/modes/` directory
3. Re-upload UI files (mode JSON files are in LittleFS)
4. Check Serial Monitor for `[Mode]` log messages

### Auto-blink not working

**Symptoms**: Eyes don't blink automatically in Follow or Auto mode

**Solutions**:
1. Check Mode Settings → Auto-Blink is enabled
2. Verify blink interval settings (default: 2-6 seconds)
3. Manual blinks reset the auto-blink timer, so frequent manual blinks delay auto-blinks

---

## Impulse System Problems

### Impulse button doesn't work

**Symptoms**: Clicking Impulse button does nothing

**Solutions**:
1. Check Impulse Settings → at least one impulse must be selected
2. Verify impulse files exist in `/impulses/` directory
3. Re-upload UI files if impulse files are missing
4. Check Console tab for `[Impulse]` error messages

### Auto-impulse not triggering

**Symptoms**: Auto-I is enabled but no impulses fire automatically

**Solutions**:
1. Check Impulse Settings → Auto-Impulse is enabled
2. Verify at least one impulse is selected in Impulse Selection
3. Check interval settings (default: 15-25 seconds)
4. Look in Console tab for `[AutoImpulse] Auto-triggered impulse` messages

### Impulse doesn't restore state

**Symptoms**: Eye position or lids stay changed after impulse

**Solutions**:
1. Check the impulse JSON file has `"restore": true`
2. Verify the impulse sequence completes (check Console for errors)
3. Custom impulses must have the restore flag set

### Impulse and blink conflict

**Symptoms**: Blinks are skipped or impulses delayed

**This is expected behavior**: Impulse waits for any in-progress blink to finish, and blinks are skipped during impulse playback. This is by design to prevent visual conflicts.

---

## Admin Lock Problems

### Forgot Admin PIN

**Solutions**:
1. Connect via device AP (192.168.4.1) - AP clients are always unlocked
2. Use Recovery Mode (http://[ip]/recovery) → Factory Reset clears the PIN

### Controls disabled but no lock icon

**Symptoms**: Can't edit settings, but no 🔒 visible

**Solutions**:
1. Check if you're rate limited (3 failed PIN attempts → 5 minute lockout)
2. Wait for lockout to expire, or connect via AP
3. Check Console tab for `[Admin] Rate limit` messages

### PIN entry not working

**Symptoms**: Enter correct PIN but stays locked

**Solutions**:
1. Ensure PIN is 4-6 digits (no letters or symbols)
2. Check for rate limiting (lockout after 3 failures)
3. Try connecting via AP to bypass PIN entirely
4. Check Console tab for auth error messages

### Recovery UI locked after firmware update

**Symptoms**: After uploading new firmware, can't upload UI files

**Solutions**:
1. Enter your PIN in the lock banner on recovery page
2. Or connect via device AP (always unlocked)
3. The device remembers your PIN across reboots

### Reboot button disabled

**Symptoms**: Reboot button grayed out

**Cause**: You're rate limited (too many failed PIN attempts)

**Solutions**:
1. Wait for lockout to expire (5 minutes)
2. Or connect via AP to reboot

---

## Recovery Procedures

### Backup Configuration First

Before factory reset, save your configuration:
1. Go to Configuration → Backup/Restore
2. Click "Download Backup"
3. Save the JSON file to your computer

Backup includes calibration, WiFi settings, mode settings, impulse settings, custom mode files, and custom impulse files.

After reset, restore via "Restore Backup" button.

**Note**: Backup is also available in Recovery Mode if main UI is inaccessible.

**Note**: Backup requires unlock if Admin PIN is set. Connect via AP (192.168.4.1) to bypass.

### Factory Reset (via Web UI)

1. Go to Configuration → System
2. Click "Factory Reset"
3. Confirm the action
4. Device will restart in AP mode with default settings

**Note**: Requires unlock if Admin PIN is set.

### Factory Reset (via Recovery Mode)

1. Access http://[device-ip]/recovery
2. Click "Factory Reset" in Danger Zone
3. Device restarts with defaults

**Note**: Factory Reset is always allowed, even when locked. This is the escape hatch for a forgotten Admin PIN.

### Factory Reset (via Serial)

If web UI is inaccessible:
1. Erase flash: `esptool.py --port /dev/ttyUSB0 erase_flash`
2. Re-upload firmware and LittleFS

### Recover from Bad UI Upload

1. Access http://[device-ip]/recovery
2. Use "Wipe UI Files" to clear corrupted files
3. Re-upload LittleFS

**Note**: Wipe UI requires unlock if Admin PIN is set. Connect via AP (192.168.4.1) to bypass.

---

## Performance Issues

### Eyes move slowly or lag

**Solutions**:
1. Check WiFi signal strength
2. Close other browser tabs
3. Reduce number of connected clients
4. Check free heap in System section

### High memory usage

**Symptoms**: Free heap below 50KB

**Solutions**:
1. Restart the device
2. Close unused WebSocket connections
3. Check for memory leaks (report if consistent)

---

## Still Having Problems?

1. Check Serial Monitor output for error messages
2. Try the recovery mode
3. Update to latest firmware
4. Open an issue on GitHub with:
   - Firmware version
   - Steps to reproduce
   - Serial Monitor output
   - Browser console errors (F12 → Console)
