# OvoEasy Incubator Controller Firmware

## Overview
ESPHome firmware for ESP32-S3 incubator controllers. Single firmware binary for fleet of ~20 units.
Controls: temperature (PID), humidity (bang-bang), water level (state machine), interior lighting.
Local UI via ILI9341 + rotary encoder. Dual logging: Home Assistant + SD card.

## Architecture
- **Framework**: ESPHome (YAML packages + custom C++ components)
- **MCU**: ESP32-S3-WROOM-1U-N16R2 (16MB flash, 2MB PSRAM)
- **Fleet model**: Single source tree, per-network-stack variant (see "Firmware Variants" below), `name_add_mac_suffix: true`, DHCP, runtime config via HA/local UI
- **Offline-first**: RTC for time, SD for logging, all control local

## Pin Mapping
```
I2C: SDA=8, SCL=9
SPI3 (display + SD): MOSI=11, MISO=13, CLK=12, CS_LCD=10, CS_SD=14
SPI2 (W5500 ethernet, optional): MOSI=35, MISO=37, CLK=36, CS=39, RST=15
MOSFET: FAN=4, HEATER=5, WATER=6, VACUUM=7, HUMIDITY=2
Display: ILI9341 RST=16, DC=47
Input: Encoder A=41, B=40, BTN=42 | Buttons A=0, B=45, C=46
LEDs: STATUS_PIX=3 (WS2812B x6), CHAMBER_PIX=1 (WS2811 x2)
Camera: TX=17, RX=18, Detect=21
Servo: SERVO_3v3=38
Aux: LED=48
```

## I2C Devices
- DS1307 RTC @ 0x68
- ADS1115 ADC @ 0x48: A0=water level, A1=thermistor, A2=aux analog header, A3=eFuse IMON
- SHT45 @ 0x44, HDC1080 @ 0x40, BME280 @ 0x76, AHT30 @ 0x38 (`aht10` platform, `variant: AHT20`)
- Env sensor priority (Auto select): SHT4x → BME280 → HDC1080 → AHT30
- Current formula: I_OUT = V_IMON [uV] / (887 * 182)  — R_ILM=887Ω, K_IMON=182µA/A

## Repository Structure
```
ovoeasy.yaml          # Production firmware — WiFi variant
ovoeasy-eth.yaml      # Production firmware — Ethernet variant (W5500-equipped units)
hardware-test.yaml    # PCB commissioning
packages/base/        # Platform, WiFi, RTC/NTP
packages/hardware/    # Sensors, display, LEDs, I/O, buses
packages/control/     # PID, bang-bang, water fill, globals
packages/ui/          # Display pages, LED feedback
packages/logging/     # HA telemetry, SD card CSV
components/           # Custom C++ (sd_logger, water_controller)
```

## Key Patterns
- **Runtime config**: globals with `restore_value: true` exposed as HA number/select entities
- **Sensor abstraction**: template sensors select control source from 3 env sensors
- **Package composition**: each package = one subsystem, composed via `!include`

## Build Commands
```bash
source /opt/miniconda3/etc/profile.d/conda.sh && conda activate esphome

# WiFi variant (default for most units)
esphome config ovoeasy.yaml        # Validate
esphome compile ovoeasy.yaml       # Build
esphome upload ovoeasy.yaml        # Flash (USB or OTA)

# Ethernet variant (W5500-equipped units only)
esphome config ovoeasy-eth.yaml
esphome compile ovoeasy-eth.yaml
esphome upload ovoeasy-eth.yaml
```

## Firmware Variants

Two firmware variants share one source tree. They differ ONLY in their `network:` package — all other packages are identical between `ovoeasy.yaml` and `ovoeasy-eth.yaml`.

- **`ovoeasy.yaml`** — WiFi variant. Uses `packages/base/network-wifi.yaml`. WiFi client + AP fallback + captive portal.
- **`ovoeasy-eth.yaml`** — Ethernet variant. Uses `packages/base/network-ethernet.yaml`. W5500 SPI ethernet only, no WiFi.

The two stacks are mutually exclusive — ESPHome rejects YAML containing both `wifi:` and `ethernet:` blocks. Per-unit choice is therefore made at flash time, not runtime. See `docs/superpowers/plans/2026-05-08-ethernet-failover-design.md` for the full rationale.

**Avoiding drift:** When adding a package, modify both top-level YAMLs in the same commit. Sanity check:
```bash
diff <(grep -v 'network:' ovoeasy.yaml) <(grep -v 'network:' ovoeasy-eth.yaml)
```
The output should be empty (only the `network:` include differs, plus the ethernet variant's extra header comment).

### Fleet Roster

Track which units use which variant.

| MAC suffix | Variant  | Location / Notes |
|------------|----------|------------------|

### Switching a unit between variants

Requires a USB reflash. An ethernet-variant unit cannot be reached via WiFi for OTA, and vice versa.

## Custom Components
- **sd_logger**: Buffered CSV to SD card. Daily rotation, 30-day retention.
- **water_controller**: State machine (IDLE->FILLING->COOLDOWN->ERROR). 120s timeout, 300s cooldown.

## KiCAD Schematics
Located at: ../../PCB/incubator_controller/

## Implementation Phases

### Phase 1 — Foundation + Hardware (complete, `ac2488a`)
Platform config, WiFi, RTC/NTP, all sensor packages (SHT45, HDC1080, BME280, ADS1115), display hardware, LEDs, I/O, SPI/I2C buses, hardware-test.yaml.

### Phase 2 — Control Systems (complete, `cc13d34`, `dc156c6`, `13a6633`)
- PID temperature control (`packages/control/climate-temperature.yaml`)
- Humidity bang-bang control (`packages/control/climate-humidity.yaml`)
- Water fill state machine custom C++ component (`components/water_controller/`)
- System state machine + status LED feedback (data-driven color tiers)
- Servo output (egg turner, GPIO38, 50Hz LEDC)

### Phase 3 — UI, Logging, Telemetry (complete, `f4f3d17`, `f65a91d`, `bf869f4`)

**3A — LVGL UI** (`f4f3d17`): 4-page display (Home, Temp, Humidity, System), MDI icon soft-keys on left edge, rotary encoder spinbox editing, contextual button actions. Spinbox boot-sync from globals included.

**3B — SD Logging + Telemetry** (`f65a91d`, `bf869f4`): Buffered CSV every 60s, daily rotation, 30-day retention, SPI bus sharing with display via `sdspi_host_init_device`. FAT LFN via `CONFIG_FATFS_LFN_HEAP`. Hot-swap detection via raw sector read. Incubation day counter, start/stop HA buttons, system event sensor.

**3C — Visual Polish** (optional, not yet done):
- Horizontal separator lines between Home page bands
- Page transition animations (`animation: MOVE_LEFT` on `lvgl.page.show`)
- Physical soft-key Y-offset tuning (requires hardware verification)

## Known Hardware Constraints
- **GPIO21 (CamConn) is NOT ADC-capable on ESP32-S3.** The voltage divider for camera detection must be read as digital GPIO, not analog. If true analog reading is needed, route through ADS1115 spare channel.
- **Strapping pins** GPIO0, GPIO3, GPIO45, GPIO46 are used (buttons/status LEDs). ESPHome warns but these are intentional PCB design choices.
- **rmt_channel** is no longer a valid option for `esp32_rmt_led_strip` in ESPHome 2025.12.x — auto-assigned.
- **invert_colors** is required for `ili9xxx` display platform in ESPHome 2025.12.x.
- **W5500 ethernet (ESP32-S3) hard-requires SPI2_HOST.** The shared display + SD SPI bus is explicitly assigned to SPI3_HOST in `packages/hardware/spi-bus.yaml` (and `components/sd_logger/sd_logger.cpp`) to free SPI2 for the optional ethernet module. The ESP32-S3 GPIO matrix routes either SPI host to any pin, so the existing PCB layout is unaffected.
