# OvoEasy Incubator Controller

ESPHome-based firmware for ESP32-S3 egg incubator controllers.

## Quick Start

1. Install ESPHome (conda env `esphome` or `pip install esphome`)
2. Copy `secrets.yaml.example` to `secrets.yaml` and fill in credentials
3. Validate: `esphome config ovoeasy.yaml`
4. Build: `esphome compile ovoeasy.yaml`
5. Flash: `esphome upload ovoeasy.yaml --device /dev/ttyUSB0`

## Fleet Deployment

Single firmware for all units. Each unit auto-names via MAC suffix.
Configure per-unit settings (setpoints, PID tuning, sensor selection) through Home Assistant or the on-device display/encoder.

## Hardware Test

Flash `hardware-test.yaml` for PCB commissioning. Exposes all I/O as simple switches for validation.

## Project Structure

- `ovoeasy.yaml` — Production firmware config
- `hardware-test.yaml` — PCB commissioning config
- `packages/` — Modular YAML packages (base, hardware, control, UI, logging)
- `components/` — Custom C++ ESPHome components
- `fonts/` — Display fonts
- `scripts/` — Build and deployment automation
