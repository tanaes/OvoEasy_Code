# Hardware Verification

Before running production firmware, verify every piece of hardware using the dedicated `hardware-test.yaml` firmware. This firmware exposes all outputs as simple web interface switches and logs all sensor values, making it easy to confirm each subsystem works before integrating them into the control loops.

## Step 1: Flash the hardware-test firmware

```bash
source /opt/miniconda3/etc/profile.d/conda.sh && conda activate esphome
esphome upload hardware-test.yaml --device /dev/cu.usbmodemXXXX
```

This requires a USB connection (OTA is not available for the test firmware on first flash).

## Step 2: Open the web interface

Find the device at `http://ovoeasy-test-XXXX.local` (note the `-test-` in the name) or check the serial log for its IP:

```bash
esphome logs hardware-test.yaml --device /dev/cu.usbmodemXXXX
```

## Step 3: Test each subsystem

Work through this checklist in order. Each row describes what to do in the web interface and what you should observe.

| # | Subsystem | Action | Expected result |
|---|-----------|--------|-----------------|
| 1 | Status LEDs (6×) | Toggle **Status LEDs** switch ON | All 6 LEDs on the top edge of the PCB illuminate |
| 2 | Chamber LEDs (2×) | Toggle **Chamber LEDs** switch ON | Both interior chamber lights illuminate |
| 3 | Heater | Toggle **Heater** switch ON for 5 seconds, then OFF | Heating element becomes warm — confirm it is the *heater*, not the fan housing |
| 4 | Fan | Toggle **Fan** switch ON | Fan spins. You should hear it and feel airflow. |
| 5 | Water pump | Toggle **Water Pump** switch ON briefly (2–3 seconds) | Pump runs. Have the inlet submerged or expect air to move. |
| 6 | Humidifier | Toggle **Humidifier** switch ON briefly | Atomizer activates (you may see/hear mist) |
| 7 | Vacuum pump | Toggle **Vacuum Pump** switch ON | Pump runs |
| 8 | Servo | Set **Egg Turner Position** to `+50`, then `-50` | Servo arm moves in both directions |
| 9 | SHT45 sensor | Check **SHT45 Temperature** and **SHT45 Humidity** | Non-NaN values (~20°C, ~40% RH typical indoors) |
| 10 | HDC1080 sensor | Check **HDC1080 Temperature** and **HDC1080 Humidity** | Non-NaN values, similar to SHT45 |
| 11 | BME280 sensor | Check **BME280 Temperature**, **Humidity**, **Pressure** | Non-NaN values; pressure ~1013 hPa at sea level |
| 12 | AHT30 sensor | Check **AHT30 Temperature** and **AHT30 Humidity** | Non-NaN values |
| 13 | Water level ADC | Check **Water Level ADC** voltage | ~0V with float switch dry; ~3V with float fully submerged |
| 14 | Rotary encoder | Rotate the encoder knob | **Encoder** value in web interface changes |
| 15 | Button A | Press the bottom-left button | **Button A** sensor shows ON briefly |
| 16 | Button B | Press the middle-left button | **Button B** sensor shows ON briefly |
| 17 | Button C | Press the top-left button | **Button C** sensor shows ON briefly |

## Gotchas

> ⚠️ **Heater vs. fan terminal mix-up**
> The heater and fan share the J7 4-position terminal block (3.81mm pitch). Heater wires go to positions 1–2, fan wires to positions 3–4. Swapping them will damage the fan's speed controller chip. Confirm you feel heat on the correct element before proceeding.

> ⚠️ **Float switch polarity**
> The water level sensor is active-high: **high voltage (~3V) = reservoir full**, low voltage (~0V) = dry. If your readings are inverted (high when dry, low when submerged), the JST-XH connector on J10 is plugged in backwards — flip it.

> ⚠️ **All env sensors read NaN**
> If every environmental sensor returns NaN, the sensor module is likely not seated correctly in its header, or an I2C address conflict exists. Re-seat the module and check the serial log for I2C errors.

> ⚠️ **Never run heater without fan**
> Do not leave the heater on for more than a few seconds without the fan running. Stagnant hot air can damage the heating element and nearby components.

## Step 4: Reflash production firmware

Once all checks pass, load the production firmware:

```bash
esphome upload ovoeasy.yaml --device /dev/cu.usbmodemXXXX
```

## Next step

→ [Network Setup](04-network.md)
