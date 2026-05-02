# OvoEasy Incubator Controller Firmware

## Overview
ESPHome firmware for ESP32-S3 incubator controllers. Single firmware binary for fleet of ~20 units.
Controls: temperature (PID), humidity (bang-bang), water level (state machine), interior lighting.
Local UI via ILI9341 + rotary encoder. Dual logging: Home Assistant + SD card.

## Architecture
- **Framework**: ESPHome (YAML packages + custom C++ components)
- **MCU**: ESP32-S3-WROOM-1U-N16R2 (16MB flash, 2MB PSRAM)
- **Fleet model**: Single firmware, `name_add_mac_suffix: true`, DHCP, runtime config via HA/local UI
- **Offline-first**: RTC for time, SD for logging, all control local

## Pin Mapping
```
I2C: SDA=8, SCL=9
SPI: MOSI=11, MISO=13, CLK=12, CS_LCD=10, CS_SD=14, ETH_CS=39
MOSFET: FAN=4, HEATER=5, WATER=6, VACUUM=7, HUMIDITY=2
Display: ILI9341 RST=16, DC=47
Input: Encoder A=41, B=40, BTN=42 | Buttons A=0, B=45, C=46
LEDs: STATUS_PIX=3 (WS2812B x6), CHAMBER_PIX=1 (WS2811 x2)
Camera: TX=17, RX=18, Detect=21
Ethernet: ETH_CS=39, ETH_RST=15
Servo: SERVO_3v3=38
Aux: LED=48
```

## I2C Devices
- DS1307 RTC @ 0x68
- ADS1115 ADC @ 0x48 (water level, thermistor, current monitor)
- SHT45 @ 0x44, HDC1080 @ 0x40, BME280 @ 0x76, AHT30 @ 0x38 (`aht10` platform, `variant: AHT20`)
- Env sensor priority (Auto select): SHT4x → BME280 → HDC1080 → AHT30
- Current formula: I_OUT = V_ILM [uV] / (887 * 182)

## Repository Structure
```
ovoeasy.yaml          # Production firmware (single image for all units)
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
esphome config ovoeasy.yaml        # Validate
esphome compile ovoeasy.yaml       # Build
esphome upload ovoeasy.yaml        # Flash (USB or OTA)
```

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
