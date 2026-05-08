# Design: OvoEasy Commissioning Guide

**Date:** 2026-05-08
**Status:** Approved
**Audience:** Collaborator commissioning a new OvoEasy incubator unit

---

## Overview

A multi-file Markdown commissioning guide for the OvoEasy incubator controller, living in `docs/commissioning/` within the `ovoeasy` firmware repo. Covers everything needed to go from an assembled, installed PCB to a fully calibrated, running incubator — using only the ESPHome CLI and the ESP32 built-in web interface (no Home Assistant required).

**Assumptions:**
- PCB is fully assembled and installed in the incubator housing (see [PCB Assembly Guide](https://github.com/tanaes/OvoEasy_Assembly/blob/main/PCB_assembly.md))
- Reader is a technical collaborator, not a firmware developer — step-by-step with gotcha callouts, links to external resources where useful
- No Home Assistant integration at this stage

---

## File Structure

```
docs/commissioning/
  README.md              ← entry point, prerequisites, ordered checklist
  01-flashing.md         ← initial USB flash
  02-ota-updates.md      ← subsequent OTA updates
  03-hardware-test.md    ← hardware-test.yaml verification walkthrough
  04-network.md          ← network setup and web interface tour
  05-pid-tuning.md       ← PID heater tuning (autotune + manual)
  06-water-fill.md       ← water system calibration
  07-ui-guide.md         ← display navigation and LED reference
```

---

## Section Specifications

### `README.md` — Entry Point

**Purpose:** Orient the reader, list prerequisites, provide an ordered checklist.

**Contents:**
- One-paragraph intro to the controller (what it does, ESP32-S3, single-binary fleet firmware)
- Prerequisites table:
  - Python environment with ESPHome installed (conda env `esphome` or `pip install esphome`)
  - USB-C cable capable of data transfer (not charge-only)
  - `secrets.yaml` filled in from `secrets.yaml.example`
  - Device physically installed with all connectors attached
- Ordered commissioning checklist with checkbox links to each section:
  1. [ ] [Initial flash](01-flashing.md)
  2. [ ] [Verify hardware](03-hardware-test.md)
  3. [ ] [Confirm network](04-network.md)
  4. [ ] [Tune PID heater](05-pid-tuning.md)
  5. [ ] [Calibrate water fill](06-water-fill.md)
  6. [ ] [Learn the UI](07-ui-guide.md)
- Brief hardware reference table: connector labels, what plugs in where (heater, fan, water pump, humidifier, float switch, env sensor module)

---

### `01-flashing.md` — Initial USB Flash

**Purpose:** Get firmware onto a brand-new device for the first time.

**Contents:**
1. Activate ESPHome environment (`conda activate esphome` or equivalent)
2. Copy and fill in `secrets.yaml` from `secrets.yaml.example` (WiFi SSID/password, OTA password)
3. Connect USB-C to the ESP32-S3 module (not the display or any peripheral connector)
4. Find the correct serial port:
   - macOS: `ls /dev/cu.usb*`
   - Linux: `ls /dev/ttyUSB* /dev/ttyACM*`
   - Windows: Device Manager → Ports
5. Run: `esphome upload ovoeasy.yaml --device /dev/cu.usbmodemXXXX`
6. Watch for "Connecting..." then progress bar — upload takes ~30–60s
7. Verify with: `esphome logs ovoeasy.yaml --device /dev/cu.usbmodemXXXX`
   - Confirm sensor readings appear, no persistent ERROR messages
8. OTA password is set at this point; future updates can be wireless

**Gotchas:**
- If upload hangs at "Connecting...", hold the BOOT button (GPIO0 / Button A on the PCB) while clicking upload, release when "Connecting..." appears
- Windows may need CP210x or CH340 USB drivers — link to driver download
- Never connect mains power to heater/fan terminals while the PCB is powered via USB alone — the MOSFETs can still switch
- A charge-only USB cable will not work — device will not appear as a serial port

---

### `02-ota-updates.md` — OTA Updates

**Purpose:** Update firmware on a device already on the network.

**Contents:**
1. Find device IP or mDNS name (see [Network](04-network.md))
2. Run: `esphome upload ovoeasy.yaml --device ovoeasy-XXXX.local`
   - Or with IP: `esphome upload ovoeasy.yaml --device 192.168.x.x`
3. OTA upload takes ~20–40s; device reboots automatically
4. Verify with `esphome logs` or check web interface

**Fallback to USB:** If OTA fails (device unreachable, stuck in error state), fall back to USB flash per section 01.

**Gotchas:**
- Device and computer must be on the same network/subnet
- OTA will fail if the device is in a crash loop — connect via USB and check logs
- The `reboot_timeout` is set to `0s` (disabled) — device will not reboot on its own if HA is absent, so a stuck OTA requires a manual power cycle

---

### `03-hardware-test.md` — Hardware Verification

**Purpose:** Validate all I/O before running production firmware, using the dedicated `hardware-test.yaml` firmware that exposes all outputs as simple switches.

**Contents:**

**Flash hardware-test firmware:**
```bash
esphome upload hardware-test.yaml --device /dev/cu.usbmodemXXXX
```

**Access the web interface** at `http://ovoeasy-test-XXXX.local` or the device IP.

**Test sequence (in order):**

| Subsystem | What to do | Expected result |
|-----------|-----------|-----------------|
| Status LEDs (6×) | Toggle "Status LEDs" switch | All 6 LEDs illuminate |
| Chamber LEDs (2×) | Toggle "Chamber LEDs" switch | Both chamber LEDs illuminate |
| Heater | Toggle "Heater" switch briefly | Element warms — confirm it's the heater, not the fan |
| Fan | Toggle "Fan" switch | Fan spins |
| Water pump | Toggle "Water Pump" switch briefly | Pump runs |
| Humidifier (atomizer) | Toggle "Humidifier" switch briefly | Atomizer activates |
| Vacuum pump | Toggle "Vacuum Pump" switch | Pump runs |
| Servo | Set "Egg Turner Position" to +50%, -50% | Servo moves both directions |
| Env sensors | Check SHT45/BME280/HDC1080/AHT30 readings | All report non-zero temp and humidity |
| Water level sensor | Check "Water Level ADC" voltage | ~0V with float dry, ~3V with float submerged |
| Rotary encoder | Rotate encoder | "Encoder" value changes |
| Buttons A/B/C | Press each | Corresponding sensor toggles in web UI |

**Gotchas:**
- Heater and fan share the same 4-pos terminal block (J7) — heater is positions 1–2, fan is 3–4. Swapping them will damage the fan controller. Verify before first power-on.
- Float switch is active-high: high voltage (~3V) = reservoir full, low voltage (~0V) = dry. If readings are inverted, check connector polarity on J10.
- Env sensor module plugs into the 4-pin header — if all four sensors read NaN, check the module is seated and I2C address conflicts are absent.
- Never run the heater continuously at 100% without airflow — always confirm the fan works first.

**When done:** Reflash production firmware:
```bash
esphome upload ovoeasy.yaml --device /dev/cu.usbmodemXXXX
```

---

### `04-network.md` — Network Setup and Web Interface

**Purpose:** Confirm the device is on the network and introduce the web interface.

**Contents:**

**Finding the device:**
- mDNS name: `http://ovoeasy-XXXX.local` where XXXX is the last 3 bytes of the MAC address (printed in boot logs)
- Alternative: check your router's DHCP client list for a device named `ovoeasy-XXXX`
- Confirm with: `esphome logs ovoeasy.yaml` — IP is printed at boot

**Web interface tour:**
- Sensor readings: all env sensors, water level, PID output, system state
- Control switches: humidifier, vacuum pump, chamber LEDs
- Number entities: temperature setpoint, humidity setpoint, PID Kp/Ki/Kd, water fill trigger, critical low level, pump fill rate, fan minimum speed
- Buttons: PID Autotune, Manual Water Fill, Reset Water Error, Start/Stop Incubation
- Select: Control Sensor (Auto/SHT45/BME280/HDC1080/AHT30)

**Offline-first note:** The device is designed to operate without a network connection. `reboot_timeout: 0s` means it will not reboot if the network is lost. All control loops run locally.

---

### `05-pid-tuning.md` — PID Heater Tuning

**Purpose:** Tune the PID temperature controller for the specific incubator chamber.

**Contents:**

**Background** (2–3 sentences + link to [ESPHome PID docs](https://esphome.io/components/climate/pid.html)):
Kp controls how aggressively the heater responds to error; Ki eliminates steady-state offset; Kd damps oscillation. Wrong values cause overshoot, oscillation, or slow response. Autotune is the recommended starting point.

**Autotune procedure:**
1. Ensure incubator is at room temperature, lid closed, fan running
2. Set target temperature to operating setpoint (37.5°C for chicken eggs) via the web interface number entity
3. Press "PID Autotune" button in web interface
4. Connect via `esphome logs` and watch the output — autotune runs for ~20–40 minutes
5. When complete, the log prints suggested Kp, Ki, Kd values
6. Enter these values in the web interface (PID Kp / Ki / Kd number entities) — they persist across reboots via `restore_value: true`

**Manual adjustment guide:**

| Symptom | Likely cause | Adjustment |
|---------|-------------|------------|
| Temperature oscillates continuously | Kp too high or Kd too low | Reduce Kp by 20%, increase Kd |
| Temperature overshoots then settles | Kd too low | Increase Kd |
| Temperature approaches setpoint slowly, never reaches it | Kp too low or Ki too low | Increase Kp first |
| Slow drift from setpoint over hours | Ki too low | Increase Ki |

**Deadband:** ±0.2°C deadband is set by default — the heater does nothing when within this range. This prevents hunting at setpoint. Widen if heater cycles too frequently; narrow if steady-state error is too large.

**Gotchas:**
- Run autotune with the lid fully closed and the incubator empty — eggs and an open lid both affect thermal mass significantly
- Autotune requires the heater and fan to be correctly wired (verify in hardware-test first)
- The fan always runs at a configurable minimum speed for air circulation — confirm it is spinning before tuning

---

### `06-water-fill.md` — Water System Calibration

**Purpose:** Configure and calibrate the water reservoir fill system.

**Contents:**

**System overview:** A binary float switch detects full/not-full. Between edges, a model estimates level by integrating pump fill rate and atomizer drain rate. The pump is binary (full on/off) — no PWM.

**Step 1 — Verify float switch polarity:**
- In the web interface, find "Water Level ADC" sensor
- With reservoir dry: should read ~0V
- With reservoir full (float submerged): should read ~3V
- If inverted, check the JST-XH connector polarity on J10 (3-pin water level sensor header)
- `float_switch_active_high: true` is set in firmware — high voltage = full

**Step 2 — Set threshold:**
- Default threshold is 1.5V (midpoint between 0V and 3V)
- Adjust in `packages/control/water-fill.yaml` if your sensor has a different voltage range

**Step 3 — Set fill trigger and critical low levels:**
- "Water Fill Trigger" (default 30%) — estimated level at which auto-fill starts
- "Water Critical Low" (default 15%) — estimated level at which WATER_ERROR is raised
- Set these via web interface number entities based on your reservoir capacity

**Step 4 — Calibrate pump fill rate:**
1. Start with reservoir at a known level (mark it)
2. Trigger "Manual Water Fill" in web interface and allow a 30-second fill
3. Measure how much the level rose
4. Calculate: `pump_fill_rate = level_change_percent / seconds`
   - Example: level rose 15% in 30s → `pump_fill_rate = 0.5 %/s`
5. Set via "Pump Fill Rate" number entity in web interface

**Step 5 — Set drain rate:**
- Observe how fast the humidity atomizer depletes the reservoir over several hours
- Rough estimate is fine — this affects model accuracy between fills, not safety
- Set via "Atomizer Drain Rate" number entity

**Step 6 — Test the full cycle:**
1. Let estimated level drop below fill trigger (or press "Manual Water Fill")
2. Confirm pump activates and water level ADC rises
3. Confirm pump stops when float switch reads full (~3V)
4. Watch state transitions on display: IDLE → FILLING → COOLDOWN → IDLE

**Gotchas:**
- Do not run the pump dry — this can damage the pump. Ensure there is water in the reservoir before triggering a manual fill.
- The 300s cooldown between fills is a safety feature — do not reduce it significantly.
- If the pump runs for 120s without the float switch tripping, the system enters ERROR_WATER state. Reset via "Reset Water Error" button after investigating.

---

### `07-ui-guide.md` — Display and LED Reference

**Purpose:** Complete reference for the on-device user interface.

**Contents:**

#### Physical Controls

| Control | Location | Function |
|---------|----------|----------|
| Button A | Bottom-left of display | Context-dependent soft-key |
| Button B | Middle-left of display | Context-dependent soft-key |
| Button C | Top-left of display | Context-dependent soft-key / Back |
| Rotary encoder | Right side of PCB | Navigate / adjust values |
| Encoder click | Press encoder shaft | Confirm selection |
| Encoder long-press (1s) | Press and hold encoder | Exit edit mode |

#### Page Navigation

```
HOME
├── Button A → TEMPERATURE page
├── Button B → HUMIDITY page
└── Button C → SYSTEM page

From any sub-page:
└── Button C → HOME
```

Soft-key labels on the left edge of the display update to reflect the current page context.

#### Home Page Layout

- **Top band:** Current temperature / setpoint
- **Middle band:** Current humidity / setpoint / water level bar
- **Bottom band:** System state / incubation day / HA notification

#### Temperature Page

- Live temperature reading
- Spinbox: temperature setpoint (press Button A "Edit SP" to focus, rotate encoder to adjust, click to confirm)
- Detail container (press Button B "Detail", rotate encoder to scroll): PID output %, sensor comparison (SHT45 / BME280 / HDC1080 / AHT30)

#### Humidity Page

- Live humidity reading
- Spinbox: humidity setpoint
- Detail container: water state, level bar, estimated level ± uncertainty, float voltage, fill count

#### System Page

- System state (NORMAL / ERROR_WATER / ERROR_TEMP / SENSOR_FAIL)
- Detail container: IP address, MAC, WiFi signal, uptime, ESP32 temperature, free heap, SD card status, HA connection, incubation day

#### Status LED Reference (6× WS2812B)

**Tier 1 — Steady color (temperature deviation):**

| Color | Meaning |
|-------|---------|
| Cyan | Chamber more than 2°C below setpoint |
| Warm white | At setpoint (±0.2°C deadband) |
| Amber | Chamber more than 2°C above setpoint |

Color interpolates smoothly across the ±2°C range.

**Tier 2 — Brief overlays (on top of Tier 1):**

| Pattern | Meaning |
|---------|---------|
| Magenta flash (300ms every 3s) | HA notification pending |
| Amber blink (200ms every 5s) | HA disconnected |

**Tier 3 — Error takeover animations (require attention):**

| Animation | Meaning | Action required |
|-----------|---------|-----------------|
| Red/orange fast alternating | Temperature runaway (>3°C above setpoint) | Check heater, check PID, check sensor |
| Blue slow pulse | Water fill timeout (pump ran 120s without float switch trip) | Check pump, check reservoir, press "Reset Water Error" |
| Dim white scanning dot | Sensor failure (all sensors NaN for >60s) | Check env sensor module connection |
