# Network Setup and Web Interface

The OvoEasy controller connects to your WiFi network using the credentials in `secrets.yaml`. Once connected, it exposes a built-in web interface for monitoring and configuration — no Home Assistant required.

## Finding the device

### Option 1: mDNS (easiest)

The device announces itself on the local network as:

```
http://ovoeasy-XXXX.local
```

where `XXXX` is derived from the last bytes of the device's MAC address. The exact name is printed in the boot log:

```bash
esphome logs ovoeasy.yaml
```

Look for a line like:
```
[I][wifi:xxx]: Connected to WiFi, IP: 192.168.1.42
[I][mdns:xxx]: mDNS hostname: ovoeasy-a1b2c3
```

### Option 2: Router DHCP table

Log into your router's admin interface and look for a device named `ovoeasy-XXXX` in the connected clients list.

### Option 3: Serial log

With USB connected:
```bash
esphome logs ovoeasy.yaml --device /dev/cu.usbmodemXXXX
```

The IP address is printed at boot.

## Web interface tour

Open `http://ovoeasy-XXXX.local` in a browser. The web interface shows all entities grouped by type:

### Sensors (read-only)
- Environmental: **SHT45**, **HDC1080**, **BME280**, **AHT30** temperature and humidity
- **Control Temperature** / **Control Humidity** — the values currently feeding the control loops
- **Active Control Sensor** — which sensor is currently selected (0=none, 1=SHT45, 2=BME280, 3=HDC1080, 4=AHT30)
- **PID Heat Output** — heater duty cycle (0–100%)
- **PID Error** — difference between setpoint and actual temperature
- **Water Level ADC** — raw float switch voltage
- **Water Estimated Level** — model-estimated reservoir level (%)
- **Water Controller State** — IDLE / FILLING / COOLDOWN / ERROR
- **System State** — NORMAL / ERROR_WATER / ERROR_TEMP / SENSOR_FAIL

### Number entities (configurable)
| Entity | Default | Description |
|--------|---------|-------------|
| Temperature Setpoint | 37.5°C | Target incubation temperature |
| Humidity Setpoint | 55% | Target relative humidity |
| PID Kp | 0.5 | Proportional gain |
| PID Ki | 0.001 | Integral gain |
| PID Kd | 0.1 | Derivative gain |
| Water Fill Trigger | 30% | Level at which auto-fill starts |
| Water Critical Low | 15% | Level at which ERROR_WATER is raised |
| Pump Fill Rate | 1.0 %/s | Model: how fast pump fills reservoir |
| Atomizer Drain Rate | 0.1 %/s | Model: how fast atomizer depletes reservoir |
| Fan Minimum Speed | — | Minimum fan duty cycle at all times |

### Buttons
| Button | Action |
|--------|--------|
| PID Autotune | Starts the PID autotune sequence (~20–40 min) |
| Manual Water Fill | Triggers one fill cycle immediately |
| Reset Water Error | Clears ERROR_WATER state after investigation |
| Start Incubation | Records today as incubation day 0 |
| Stop Incubation | Clears incubation day counter |

### Select
- **Control Sensor**: Auto / SHT45 / BME280 / HDC1080 / AHT30
  - **Auto** (default) picks the first available sensor in priority order: SHT45 → BME280 → HDC1080 → AHT30

## Offline-first design

The device is configured with `reboot_timeout: 0s`. It will **not** reboot if it loses WiFi or Home Assistant connectivity. All temperature, humidity, and water control loops run locally regardless of network state. This means a deployed incubator keeps running through network outages.

## Next step

→ [PID Tuning](05-pid-tuning.md)
