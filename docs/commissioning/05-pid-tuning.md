# PID Heater Tuning

The heater is controlled by a PID (Proportional-Integral-Derivative) controller. PID controllers need to be tuned for each physical chamber because thermal mass, insulation, and heater power vary between builds. Well-tuned PID gives stable temperature with minimal overshoot. Poorly tuned PID causes oscillation or slow response.

For a plain-English explanation of PID, see the [ESPHome PID Climate documentation](https://esphome.io/components/climate/pid.html).

## Before you start

- Complete [Hardware Verification](03-hardware-test.md) — the fan and heater must both be correctly wired
- The incubator should be empty (no eggs)
- Close the lid fully — thermal mass changes significantly with the lid open
- Confirm the fan is spinning (the fan runs continuously at minimum speed)

## Method 1: Autotune (recommended)

ESPHome's PID autotune runs the heater through a controlled oscillation cycle and calculates good starting parameters automatically.

### 1. Set the target temperature

In the web interface, set **Temperature Setpoint** to your operating temperature (37.5°C for chicken eggs).

### 2. Start autotune

Press the **PID Autotune** button in the web interface.

### 3. Monitor via serial log

```bash
esphome logs ovoeasy.yaml --device ovoeasy-XXXX.local
```

Autotune will run the heater on and off several times, measuring the system response. This takes **20–40 minutes**. Do not open the lid or change settings during this time.

### 4. Read the results

When autotune completes, the serial log prints suggested values:

```
[I][pid.autotune:xxx]: PID Autotune finished!
[I][pid.autotune:xxx]:   Kp=0.423, Ki=0.004, Kd=2.841
```

### 5. Apply the values

In the web interface, set **PID Kp**, **PID Ki**, and **PID Kd** to the suggested values. These are saved to flash with `restore_value: true` — they persist across reboots.

### 6. Verify stability

Watch the temperature for 30 minutes. It should settle within ±0.3°C of setpoint without oscillating.

## Method 2: Manual tuning

If autotune doesn't converge (uncommon), tune manually using the symptoms below.

### Starting point
Set: Kp=0.5, Ki=0.001, Kd=0.1

### Symptom → adjustment table

| Symptom | Likely cause | Adjustment |
|---------|-------------|------------|
| Temperature oscillates continuously | Kp too high, or Kd too low | Reduce Kp by 20%; increase Kd |
| Temperature overshoots setpoint then settles | Kd too low | Increase Kd by 50% |
| Temperature rises slowly, never quite reaches setpoint | Kp too low, or Ki too low | Increase Kp first; if still slow after 30 min, increase Ki |
| Temperature reached setpoint once but drifts away over hours | Ki too low | Increase Ki by 2× |

Make one change at a time and wait 20 minutes to assess the result.

## Deadband

The controller has a ±0.2°C deadband — the heater does nothing when temperature is within this window of setpoint. This prevents rapid on/off cycling at setpoint.

- If the heater cycles very rapidly at setpoint, **widen** the deadband (edit `packages/control/climate-temperature.yaml`, `threshold_high`/`threshold_low`)
- If steady-state error is larger than you'd like, **narrow** the deadband

## Gotchas

> ⚠️ **Autotune with an open lid or eggs inside**
> Both significantly change the thermal dynamics. Autotune results will not transfer well to closed-lid operation with a full clutch. Always autotune in the exact operating configuration.

> ⚠️ **Fan must be running**
> The heater will create hot spots without airflow. If the fan is not spinning, stop and resolve the wiring issue before tuning.

> ⚠️ **PID Kd and sensor noise**
> High Kd values amplify sensor noise. If the heater chatters with high Kd, the sensor readings may be noisy — check the `sliding_window_moving_average` filter size in `packages/hardware/sensors-env.yaml`.

## Next step

→ [Water Fill Calibration](06-water-fill.md)
