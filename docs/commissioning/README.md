# OvoEasy Incubator — Commissioning Guide

This guide walks through commissioning a new OvoEasy incubator controller board from first flash to calibrated operation. It assumes the PCB is fully assembled and installed in the incubator housing. For PCB assembly, see the [PCB Assembly Guide](https://github.com/tanaes/OvoEasy_Assembly/blob/main/PCB_assembly.md).

The OvoEasy controller is an ESP32-S3-based board running ESPHome firmware. All setup and calibration in this guide uses the ESPHome CLI and the device's built-in web interface — no Home Assistant required at this stage.

## Prerequisites

| Requirement | Notes |
|-------------|-------|
| ESPHome installed | `pip install esphome` or activate the `esphome` conda env |
| USB-C data cable | Must support data transfer — charge-only cables will not work |
| `secrets.yaml` filled in | Copy `secrets.yaml.example` → `secrets.yaml` and add your WiFi credentials |
| Device installed | PCB mounted, all connectors attached (heater, fan, water pump, sensors) |

## Commissioning Checklist

Complete these in order:

- [ ] [1. Initial flash](01-flashing.md) — Load firmware onto the device for the first time
- [ ] [2. Hardware verification](03-hardware-test.md) — Confirm all I/O works before running production firmware
- [ ] [3. Network connection](04-network.md) — Find the device on your network and tour the web interface
- [ ] [4. PID tuning](05-pid-tuning.md) — Tune the heater controller for your chamber
- [ ] [5. Water fill calibration](06-water-fill.md) — Calibrate the water reservoir system
- [ ] [6. UI reference](07-ui-guide.md) — Learn the on-device display and LED indicators

OTA update procedure (for subsequent firmware updates): [02-ota-updates.md](02-ota-updates.md)

## Hardware Reference

| Connector | Location | Connects to |
|-----------|----------|-------------|
| J7 (4-pos, 3.81mm) | Front, top-right | Heater (pos 1–2) and Fan (pos 3–4) |
| J3 (4-pos, 3.5mm) | Front, top-left | Water pump, humidifier, vacuum |
| J10 (3-pos JST-XH) | Back | Float switch / water level sensor |
| J5 (6-pos JST-PH) | Back | Environmental sensor module |
| J6/J13 (4-pos JST-PH) | Back | Chamber LEDs |
| J4 (4-pos JST-JQ) | Back | Display (ILI9341) |
| J8/J9 (3-pos JST-PH) | Back | Buttons and encoder |

> ⚠️ **Before first power-on:** Verify heater and fan are on the correct terminals. Swapping them can damage the fan speed controller.
