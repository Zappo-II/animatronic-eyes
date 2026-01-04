# Calibration Guide

Proper calibration ensures smooth, natural eye movements without mechanical strain.

## Why Calibration Matters

Each animatronic build is slightly different:
- Servo mounting angles vary
- 3D print tolerances differ
- Linkage lengths may not be exact

Calibration maps the software's -100 to +100 range to your specific servo's physical limits.

**Warning**: Improper calibration can damage or destroy your servos and mechanical linkages. Always stay within physical limits and never force movement past resistance.

## Understanding Calibration Values

| Parameter | Description | Range |
|-----------|-------------|-------|
| **Min** | Servo position for -100 (e.g., look left/down/close) | 0-180° |
| **Center** | Servo position for 0 (neutral/centered) | 0-180° |
| **Max** | Servo position for +100 (e.g., look right/up/open) | 0-180° |
| **Invert** | Reverse servo direction | On/Off |

**Default values**: Min=89°, Center=90°, Max=91° (very small range for safety)

## Calibration Process

### Before You Start

1. Power the servos properly (USB power may be insufficient for 6 servos)
2. Ensure mechanical linkages are connected but not over-tightened
3. Have the web UI open to the **Calibration** tab

### Step-by-Step

For each servo, follow these steps in order:

#### 1. Set Safe Test Range

Before exploring full range, create a small safe zone to test direction:

1. Set **Min** to 85° (5° below default center)
2. Set **Max** to 95° (5° above default center)
3. Click **Save**

This gives you a safe 10° range to verify movement direction without risking mechanical damage.

#### 2. Check Direction

1. Move the **Test Slider** from center toward Max
2. Observe which direction the servo moves
3. If the movement is opposite to expected, toggle **Invert** on

For example, if increasing gaze X should move the eye right but it moves left, enable invert.

**Note**: Toggling invert takes effect immediately (no Save required) and moves the servo to center for safety.

#### 3. Find Minimum Position

1. Slowly decrease the **Min** value and use the Test Slider to test
2. Watch the mechanical movement carefully
3. Stop when you reach the physical limit (don't force it!)
4. Set **Min** slightly inside the limit for safety margin
5. Click **Save**

**Warning**: Going beyond physical limits can strip servo gears or damage linkages.

#### 4. Find Maximum Position

1. Slowly increase the **Max** value and use the Test Slider to test
2. Stop at the physical limit
3. Set **Max** slightly inside the limit for safety margin
4. Click **Save**

#### 5. Find Center Position

Now that you know the full range:

1. Use the **Test Slider** to find the neutral position
2. For eye X/Y servos: pupil should look straight ahead
3. For eyelids: relaxed, neutral open position (not wide open)
4. Set **Center** to this value
5. Click **Save**

#### 6. Repeat for All Servos

Calibrate in this order for best results:
1. Left Eye X
2. Left Eye Y
3. Right Eye X
4. Right Eye Y
5. Left Eyelid
6. Right Eyelid

### Verify Calibration

1. Go to the **Control** tab
2. Use the gaze pad to move eyes around
3. Check that:
   - Eyes move smoothly to corners
   - Eyes are centered when gaze pad shows X=0, Y=0
   - Eyelids open and close fully
   - No grinding or skipping sounds

## Per-Servo Tips

### Eye X (Horizontal)

- Range typically 45°-135°
- Watch for eyeball hitting the socket at extremes
- Left and right eye should have similar ranges

### Eye Y (Vertical)

- Usually smaller range than X (eyes don't look as far up/down)
- Range typically 70°-110°
- Watch for eyelids interfering at extreme up/down positions

### Eyelids

- Min = fully closed
- Max = fully open
- Center = relaxed/neutral (not wide open)
- Check that eyelids clear the eyeball when opening
- Ensure eyelids meet properly when closing

## Troubleshooting Calibration

### Servo jitters or oscillates

- The position is at or past the physical limit
- Pull back the Min/Max values

### Eye doesn't reach corner

- Increase the Min/Max range
- Check if mechanical linkage is limiting movement

### Eyes don't look the same direction

- Verify both eye X servos have similar calibration
- One may need Invert toggled

### Eyelids don't close completely

- Adjust Min value until lids touch
- Check physical linkage for obstructions

### Servos are hot

- They're working against mechanical resistance
- Find and fix the obstruction
- Reduce movement range

## Saving and Restoring

- Calibration is saved to NVS flash automatically when you click **Save**
- Settings persist across reboots
- Use **Factory Reset** to return to defaults (will lose all calibration!)

## Recalibration

You may need to recalibrate if:
- You disassemble and reassemble the mechanism
- Servo linkages slip or are adjusted
- Servos are replaced
- The mounting changes

## Default Pin Configuration

| Servo | Index | Default GPIO |
|-------|-------|--------------|
| Left Eye X | 0 | 32 |
| Left Eye Y | 1 | 33 |
| Left Eyelid | 2 | 25 |
| Right Eye X | 3 | 26 |
| Right Eye Y | 4 | 27 |
| Right Eyelid | 5 | 14 |

Pins can be changed in the calibration card settings if your wiring differs.
