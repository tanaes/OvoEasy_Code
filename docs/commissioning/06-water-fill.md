# Water Fill System Calibration

The OvoEasy uses an automated water reservoir fill system to maintain humidity. A binary float switch detects when the reservoir is full. Between switch events, the firmware estimates the current level by integrating a pump fill rate (how fast the pump adds water) and an atomizer drain rate (how fast the humidifier consumes it). The pump is binary — fully on or fully off.

## Step 1: Verify float switch polarity

The float switch should read **high voltage (~3V) when submerged (full)** and **low voltage (~0V) when dry**.

1. Open the web interface and find the **Water Level ADC** sensor
2. With the reservoir empty (float switch hanging in air): confirm reading is ~0V
3. Submerge the float switch fully in water: confirm reading rises to ~3V

If the readings are inverted (high when dry, low when submerged), you will need to adjust
the logic in firmware. 

> The firmware is configured with `float_switch_active_high: true` (high voltage = full). This matches hardware where the float switch pulls the line high when submerged. Do not change this setting unless you replace the sensor with a different type.

## Step 2: Confirm the threshold voltage

The firmware compares the ADC reading against a threshold (default **1.5V**) to decide full vs. not-full.

- If your sensor reads ~0V dry and ~3V full, the default 1.5V midpoint is correct
- If your sensor has a different voltage range, adjust `float_switch_threshold` in `packages/control/water-fill.yaml` and reflash

## Step 3: Set fill trigger and critical low levels

These are set via the web interface and saved to flash:

| Entity | Default | Meaning |
|--------|---------|---------|
| **Water Fill Trigger** | 30% | Auto-fill starts when estimated level drops to this |
| **Water Critical Low** | 15% | `ERROR_WATER` is raised if level drops to this |

Adjust these based on your reservoir's usable capacity. A smaller reservoir needs a higher fill trigger to avoid running dry between fills.

## Step 4: Calibrate pump fill rate

The pump fill rate tells the model how fast the pump adds water to the reservoir. This affects how the system behaves between float switch readings.

1. Fill the reservoir to a known starting point (e.g., mark the outside)
2. In the web interface, press **Manual Water Fill** — time how long the pump runs until it stops (the float switch should trip, stopping the pump)
3. Measure how much the water level rose (as a percentage of total reservoir capacity)
4. Calculate:

```
pump_fill_rate (%/s) = level_rise (%) / fill_duration (seconds)
```

**Example:** Level rose from 25% to 100% (75%) in 90 seconds → `pump_fill_rate = 75 / 90 = 0.83 %/s`

5. Set **Pump Fill Rate** in the web interface to the calculated value

## Step 5: Set atomizer drain rate

The drain rate is how fast the humidifier atomizer depletes the reservoir.

1. Note the estimated level at a known time
2. Run the humidifier for several hours with the water pump disabled (uncheck auto-fill or set fill trigger very low)
3. Note how much the level dropped and over what time period
4. Calculate: `drain_rate (%/s) = level_drop (%) / duration (seconds)`

A rough estimate is acceptable — this parameter affects the model's estimate accuracy between fills, not safety. The float switch corrects the model at every fill event.

Set **Atomizer Drain Rate** in the web interface.

## Step 6: Test the full fill cycle

1. Let the estimated level drop below the fill trigger (or press **Manual Water Fill**)
2. Confirm in the web interface and on the display:
   - Water Controller State: **FILLING**
   - Water Level ADC rises as the pump runs
3. When the float switch reads full (~3V), confirm:
   - Pump stops
   - State transitions to **COOLDOWN** (300 seconds)
   - State then returns to **IDLE**

Watch the **Water Model Uncertainty** sensor — it should decrease after each fill cycle as the model anchors to the float switch reading.

## Gotchas

> ⚠️ **300-second cooldown is a safety feature**
> The firmware enforces a 5-minute cooldown between fills to prevent pump overheating and rapid cycling. Do not reduce this significantly.

> ⚠️ **ERROR_WATER state**
> If the pump runs for 120 seconds without the float switch tripping, the system assumes a fault (pump failure, float switch stuck, or very low reservoir) and enters ERROR_WATER. The status LEDs pulse blue. Investigate the cause, then press **Reset Water Error** in the web interface.

## Next step

→ [UI Reference](07-ui-guide.md)
