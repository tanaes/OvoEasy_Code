# UI Reference: Display and LEDs

The OvoEasy controller has a 2.4" ILI9341 touchscreen display (non-touch — operated via buttons and encoder), three physical soft-key buttons, a rotary encoder, and two sets of indicator LEDs.

## Physical Controls

| Control | Physical Location | Function |
|---------|------------------|----------|
| **Button A** | Bottom-left edge of display | Context-dependent soft-key |
| **Button B** | Middle-left edge of display | Context-dependent soft-key |
| **Button C** | Top-left edge of display | Context-dependent soft-key / Back |
| **Rotary encoder** | Right side of PCB | Scroll / adjust value |
| **Encoder click** | Press encoder shaft | Confirm / enter edit |
| **Encoder long-press** (hold 1s) | Press and hold encoder shaft | Exit edit mode |

The display is mounted rotated 180° — Button C is physically at the top but corresponds to the top-left label on screen.

## Page Navigation

```
HOME PAGE
├── Button A  ──→  TEMPERATURE page
├── Button B  ──→  HUMIDITY page
└── Button C  ──→  SYSTEM page

From any sub-page:
└── Button C  ──→  HOME PAGE
```

The three soft-key labels on the left edge of the display update to show what each button does on the current page.

---

## Home Page

The home page shows a quick overview of the incubator state in three horizontal bands.

**Top band — Temperature:**
- Large: current temperature (°C)
- Small: temperature setpoint

**Middle band — Humidity and Water:**
- Large: current humidity (%RH)
- Small: humidity setpoint
- Bar: estimated water reservoir level
- Small: water level percentage

**Bottom band — System:**
- System state text (NORMAL / ERROR_WATER / ERROR_TEMP / SENSOR_FAIL)
- Incubation day counter (e.g., "Day 7")
- HA notification indicator (shows `! NOTIFICATION` in magenta when active)

---

## Temperature Page

Reached from Home with **Button A**.

- **Top:** Live temperature reading
- **Spinbox:** Temperature setpoint
  - Press **Button A** ("Edit SP") to focus the spinbox
  - Rotate encoder to adjust (0.1°C steps)
  - Click encoder to confirm
  - Long-press encoder to exit edit without saving (reverts)
- **Detail container** (press **Button B** to focus, rotate encoder to scroll):
  - PID output % (heater duty)
  - Individual sensor readings: SHT45 / BME280 / HDC1080 / AHT30 (temp / humidity)

---

## Humidity Page

Reached from Home with **Button B**.

- **Top:** Live humidity reading
- **Spinbox:** Humidity setpoint (1% steps)
  - Same edit controls as temperature spinbox
- **Detail container:**
  - Water controller state
  - Water level bar and estimated level ± uncertainty
  - Float switch voltage (raw ADC)
  - Total fill count since last boot

---

## System Page

Reached from Home with **Button C**.

- **Top:** System state (large text)
- **Detail container:**
  - IP address
  - MAC address
  - WiFi signal strength (dBm)
  - Uptime (days, hours, minutes)
  - ESP32 internal temperature
  - Free heap memory (KB)
  - SD card status
  - HA connection status
  - Incubation day / 21

---

## Status LEDs (6× WS2812B)

Six RGB LEDs run along the top edge of the PCB. Their color and animation encode the system state using a three-tier priority system.

### Tier 1 — Temperature deviation (steady color, always shown)

The LEDs glow a color that reflects how far the chamber temperature is from setpoint. This is the normal operating display.

| Color | Meaning |
|-------|---------|
| **Cyan** | Chamber is >2°C *below* setpoint |
| Cyan → Warm white | Gradually warming toward setpoint |
| **Warm white** | At setpoint (within ±0.2°C deadband) |
| Warm white → Amber | Gradually above setpoint |
| **Amber** | Chamber is >2°C *above* setpoint |

Color interpolates smoothly across the ±2°C range. Cyan at startup means the heater is warming up; amber means the chamber is overshooting.

### Tier 2 — Communication overlays (brief flashes on top of Tier 1)

These flashes appear briefly over the Tier 1 color and do not indicate a problem.

| Pattern | Meaning |
|---------|---------|
| **Magenta flash** (300ms every 3 seconds) | A Home Assistant notification is pending. Acknowledge it via the HA interface. |
| **Amber blink** (200ms every 5 seconds) | Home Assistant is disconnected. Normal during commissioning without HA; safe to ignore if you are not using HA. |

### Tier 3 — Error animations (take over all LEDs, require attention)

Error animations replace the temperature color entirely and require human intervention.

| Animation | Meaning | What to do |
|-----------|---------|------------|
| **Red/orange fast alternating** (500ms cycle) | Temperature runaway — chamber is >3°C above setpoint | Check heater wiring; check sensor readings; check PID settings. Power off if temperature continues rising. |
| **Blue slow pulse** (2-second breathing cycle) | Water fill timeout — pump ran for 120s without float switch tripping | Check pump is running; check water reservoir has water; check float switch voltage; press **Reset Water Error** after resolving. |
| **Dim white with single scanning dot** (~1.5s sweep) | Sensor failure — all environmental sensors have been reporting NaN for >60 seconds | Check env sensor module connection; check serial log for I2C errors; re-seat module. |

---

## Chamber LEDs (2× WS2811)

Two larger LEDs illuminate the interior of the incubator chamber. They are for visibility when checking eggs — they have no diagnostic meaning.

Toggle them from the web interface (**Chamber LEDs** switch) or, if configured, via a button action on the display.
