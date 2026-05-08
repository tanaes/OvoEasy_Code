# Initial Flashing

This page covers loading the OvoEasy firmware onto a brand-new device for the first time over USB.

## Prerequisites

- ESPHome environment active (`conda activate esphome` or `pip install esphome`)
- `secrets.yaml` filled in — copy from `secrets.yaml.example`:

```yaml
wifi_ssid: "YourNetworkSSID"
wifi_password: "YourWiFiPassword"
ap_password: "ovoeasy-config"
ha_api_key: "YOUR-32-BYTE-BASE64-KEY-HERE"
ota_password: "secure-ota-password"
```

- USB-C data cable connected to the ESP32-S3 module's USB port (the port on the module itself, not a peripheral connector)

## Steps

### 1. Find the serial port

**macOS:**
```bash
ls /dev/cu.usb*
```

**Linux:**
```bash
ls /dev/ttyUSB* /dev/ttyACM*
```

**Windows:** Open Device Manager → Ports (COM & LPT). Install [CP210x drivers](https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers) or [CH340 drivers](https://www.wch-ic.com/downloads/CH341SER_EXE.html) if no port appears.

### 2. Flash the firmware

```bash
source /opt/miniconda3/etc/profile.d/conda.sh && conda activate esphome
esphome upload ovoeasy.yaml --device /dev/cu.usbmodemXXXX
```

Replace `/dev/cu.usbmodemXXXX` with your actual port. The upload takes 30–60 seconds. You will see a progress bar during flashing.

### 3. Verify the flash

```bash
esphome logs ovoeasy.yaml --device /dev/cu.usbmodemXXXX
```

Look for:
- Sensor readings (temperature, humidity) appearing every 10 seconds
- No persistent `ERROR` or `FAILED` messages
- A line like `WiFi connected, IP: 192.168.x.x`

Press Ctrl+C to exit the log viewer.

## Gotchas

> ⚠️ **Upload hangs at "Connecting..."**
> Hold the **BOOT button** (Button A — bottom-left of the display) while the upload command is running. Release it as soon as "Connecting..." appears in the terminal. This manually triggers bootloader mode.

> ⚠️ **Charge-only USB cable**
> The device will not appear as a serial port with a charge-only cable. Use a cable you know carries data (e.g., one that works for file transfer with a phone).

> ⚠️ **Mains power during USB flash**
> The MOSFET outputs (heater, fan, pump) can switch even when the board is powered only via USB. Do not connect mains-voltage loads to the terminal blocks during development and flashing.

> ⚠️ **Windows drivers**
> The ESP32-S3 USB bridge uses a CP210x or CH340 chip. If the device does not appear in Device Manager, install the appropriate driver from the links above.

## Next step

→ [Hardware Verification](03-hardware-test.md)
