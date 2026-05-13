# Humidity Control: Approach Comparison

**Date:** 2026-05-12
**Status:** Design review (no code changes in this branch)
**Decides:** what `packages/control/climate-humidity.yaml` should be after the water-interlock removal and the 10s-filter work.

## 1. Current state

`packages/control/climate-humidity.yaml` instantiates ESPHome's `thermostat` climate platform as `humidity_climate` (lines 5-39). Temperature is wired to `control_temp` and `humidity_sensor` to `control_humidity`, but the temperature side is a deliberate no-op: `idle_action` and `heat_action` just log (lines 14-17), because temperature is owned by the separate `pid` climate in `climate-temperature.yaml`. The humidity side drives `humidity_output` (GPIO2 atomizer MOSFET) via `humidity_control_humidify_action` (lines 18-31), gated on `water_ctrl.is_critical_low()`. The setpoint is bootstrapped from the `target_humidity` global at `on_boot` priority 600 (lines 42-49) and resynced every 30 s by a polled-diff lambda (lines 52-62), because no native binding exists from a `number` entity into a climate's target_humidity. The `Humidity Setpoint` HA number entity in `packages/control/globals.yaml` lines 103-117 is the canonical source of truth; `humidity_climate.target_humidity` is a downstream cache.

Hysteresis is written `humidity_hysteresis: 2%` (line 36) and is **effectively broken** per esphome/esphome#12697. ESPHome's `cv.percentage` validator (`config_validation.py` line 1408-1414) divides any `"N%"` string by 100 before storage, so the C++ field `humidity_hysteresis_` (`thermostat_climate.h` line 403) ends up as `0.02`. The comparator at `thermostat_climate.cpp` line 1121 then checks `current_humidity < target_humidity - 0.02`, where `current_humidity` is in normal percent units (e.g. 55.0). Result: 0.02 % deadband instead of 2 %. The issue is closed-stale; no fix has shipped. The `min_idle_time: 30s` / `min_heating_run_time: 10s` on the climate are currently the only thing keeping the atomizer from chattering at ~1 Hz.

## 2. ESPHome native options

I read `/opt/miniconda3/envs/esphome/lib/python3.13/site-packages/esphome/components/` directly. Of the climate platforms found by `find … -name climate.py`, only three are general-purpose: `thermostat`, `bang_bang`, and `pid`. The other 20+ (`mitsubishi`, `daikin`, `haier`, `midea`, `coolix`, …) are HVAC IR-remote drivers and are not applicable. There is **no `humidifier` climate platform, no `generic_thermostat`, no `humidity_controller`**. The only humidity-named component is `absolute_humidity` (`absolute_humidity/sensor.py`), which is a *sensor* that converts RH+T into g/m³ — not a controller.

### 2a. `thermostat` (current choice)

- **What it is:** Combined temp + humidity bang-bang with optional dehumidify/humidify actions. Heat/cool deadbands, hysteresis, min on/off times, multiple presets.
- **Wire-up:** What we already have, minus the water-interlock branch (other branch is removing it). Six no-op `heat_action`/`idle_action` lines stay because the schema requires at least one temp action.
- **Pros:** Native HA climate entity with humidity setpoint slider, target reporting, action state (`humidifying` / `off`); built-in min-on/min-off timers.
- **Cons:** (a) The `humidity_hysteresis: 2%` schema bug. (b) Pretending to be a thermostat when we don't actually control temperature here — confusing for anyone reading the YAML. (c) The 30 s polling lambda to keep target_humidity in sync with the global is a workaround for the fact that thermostat doesn't accept a templated/global target. (d) The visual block carries temperature limits (lines 38-39) that have no meaning for our humidity-only use.
- **Verdict:** Viable only with the runtime `set_humidity_hysteresis(2.0)` workaround (section 4).

### 2b. `bang_bang`

- **What it is:** Heat/cool bang-bang with a high/low temperature window. Optionally accepts a `humidity_sensor` (`bang_bang/climate.py` line 25).
- **Wire-up:** Cannot drive a humidifier. Inspection of `bang_bang_climate.h` shows it exposes `set_humidity_sensor` (line 27) but has no target-humidity field, no `humidify_action`, no humidify trigger. The humidity sensor is purely for HA reporting of `current_humidity` on the climate entity.
- **Pros:** None for our problem.
- **Cons:** Cannot control humidity at all. Adopting it would mean still needing a separate control loop.
- **Verdict:** Not viable.

### 2c. `pid`

- **What it is:** PID controller producing a 0-100 % output, applied to a temperature setpoint.
- **Wire-up:** Conceptually you could feed it `control_humidity` as the "temperature" sensor and target 55, then drive a slow-PWM output. But the atomizer is a binary MOSFET load, not a proportional one; we already pulse it with `min_idle_time`/`min_heating_run_time`. PID over a slow PWM is doable but adds tuning burden (Kp/Ki/Kd in %-RH units) for no functional gain over bang-bang in a closed chamber with a single-speed atomizer.
- **Pros:** Smooth output if the actuator were proportional; no schema bugs.
- **Cons:** Wrong control type for a binary actuator; doubles our PID-tuning surface area; HA-side label says "temperature" everywhere.
- **Verdict:** Not viable for this hardware.

### 2d. Other

`absolute_humidity` is a derived sensor, not a controller. No other ESPHome built-in is relevant. Confirmed by listing all `climate.py` under `site-packages/esphome/components/`.

## 3. Roll-your-own bang-bang `interval` lambda

Replace the entire `thermostat` block with a plain `interval:` loop that reads sensors and globals and toggles the output. Sketch (worktree placement: `packages/control/climate-humidity.yaml`, replacing the climate + on_boot + interval blocks):

```yaml
# Adds to packages/control/globals.yaml:
#   - id: humidity_hysteresis_pct
#     type: float
#     restore_value: true
#     initial_value: "2.0"
# Plus a "Humidity Hysteresis" number entity (entity_category: config).

binary_sensor:
  - platform: template
    id: humidifier_active
    name: "Humidifier Active"
    lambda: "return id(humidity_output).get_state() > 0.0f;"

interval:
  - interval: 5s
    then:
      - lambda: |-
          const float rh = id(control_humidity).state;
          const float sp = id(target_humidity);
          const float h  = id(humidity_hysteresis_pct);
          if (isnan(rh) || isnan(sp)) {
            id(humidity_output).turn_off();
            return;
          }
          const bool on = id(humidity_output).get_state() > 0.0f;
          if (!on && rh < sp - h) id(humidity_output).turn_on();
          else if (on && rh > sp + h) id(humidity_output).turn_off();
```

- **Pros:** Explicit. No schema units bug — hysteresis is a float in percent, written once, read once. Hysteresis is runtime-tunable from HA without recompile. No 30 s polling sync — the global *is* the setpoint. No fake `heat_action`. Removing the water interlock is one deleted lambda line, not a structural change. Easier to add features (anti-short-cycle timer, sensor-stale fallback, hysteresis asymmetry for "humidify hard, release slowly") because we own the state machine.
- **Cons:** Loses the climate.xxx HA entity — but `Humidity Setpoint` already exists as a `number` (globals.yaml lines 103-117), so the setpoint slider is **not** lost. What is lost is the climate's `action` field showing `humidifying` / `idle` in HA, and the unified target/current pair on one climate card. Replacement: a 6-line `binary_sensor` template (`humidifier_active`) gives HA the same on/off signal that automations would use. Mode-off (humidifier disable) becomes a separate `switch` if we want one, but for a single-purpose incubator that runs 24/7 it's arguably not needed at all.
- **Sketch length:** ~20 YAML lines including the new global + number, vs ~60 today.

## 4. Hybrid: keep `thermostat`, force hysteresis in C++

```yaml
esphome:
  on_boot:
    priority: 600
    then:
      - lambda: id(humidity_climate).set_humidity_hysteresis(2.0);
```

The setter (`thermostat_climate.cpp` line 1377-1378) clamps to `[0, 100]` and writes the field directly, bypassing `cv.percentage`. Delete `humidity_hysteresis: 2%` from the YAML so a future ESPHome fix doesn't double-apply.

- **Pros:** Minimum-diff fix. Keeps the HA climate entity and the 30 s polling sync (unchanged). Removing the water-interlock branch trivially. One-line patch.
- **Cons:** Hack that bypasses the schema and only works because the C++ setter happens to accept percent units. If/when esphome/esphome#12697 is fixed upstream, the YAML and lambda would *both* apply — either silently double-correcting (harmless if both are 2.0) or actively wrong (if the fix changes semantics, e.g. clamping `0..1`). We would have to remember to remove the workaround at upgrade time. Doesn't address the "thermostat platform with no-op temp side" awkwardness or the polling sync.

## 5. Recommendation

**Build option 3 (roll-your-own interval lambda).** Add `humidity_hysteresis_pct` as a `restore_value: true` global plus a HA `number` entity (matching the pattern already used for setpoints, PID gains, and float-switch threshold), and replace the entire `thermostat` climate block with a ~10 s interval lambda — or 5 s with hysteresis carrying the anti-chatter load, which is what the 10 s effective-filter work is about anyway.

Trade-off: we give up the `climate.humidity_climate` entity in HA, but the `Humidity Setpoint` number entity has been the actual user-facing knob since `globals.yaml` was written — the climate's slider was redundant from day one, evidenced by the polling-diff sync hack required to keep them in step. We gain: explicit code that says what it does, no upstream-schema landmine, runtime-tunable hysteresis exposed via the same pattern as every other tuning parameter, trivial extensibility for the 10 s filter and water-interlock-removal work happening in parallel branches, and one less "this is a thermostat pretending to be a humidifier" footgun for the next person reading the YAML. The fleet is 20 units of single-purpose incubators with custom C++ components already in tree — we are not preserving optionality for some future general-purpose climate use, and the YAML-vs-C++ fence isn't real for this project.

If for some reason we wanted the lowest-risk patch this week (e.g. a unit is shipping tomorrow), option 4 is the bridge: one-line `set_humidity_hysteresis(2.0)` and remove `humidity_hysteresis: 2%`. But it's a stopgap, not the destination.

## 6. Out of scope

- A custom `external_components/humidity_controller/` C++ component. Considered and rejected: this control loop is ~10 lines of business logic, and adding a fourth custom component (`sd_logger`, `water_controller`, …) for it is overkill. If the loop grows hooks, alarms, and predictive logic, revisit then.
- Adding a generic PWM driver for a future proportional atomizer. Not applicable to current hardware (binary MOSFET).
- Re-introducing the water-critical-low interlock here. The other branch removes it deliberately; this review respects that decision.
- Predictive/feed-forward humidity control coupled to chamber temperature setpoint changes. Worth a separate design review once basic bang-bang is solid and we have logged data.

## References

- Current YAML: `packages/control/climate-humidity.yaml`
- Setpoint global + HA number entity: `packages/control/globals.yaml` lines 9-12, 103-117
- ESPHome bug: esphome/esphome#12697 (closed-stale)
- ESPHome schema validator: `config_validation.py` line 1408-1414 (`cv.percentage` returns 0..1 float)
- ESPHome thermostat C++ comparator: `thermostat/thermostat_climate.cpp` lines 1107-1131
- ESPHome thermostat hysteresis setter: `thermostat/thermostat_climate.cpp` lines 1377-1378
- ESPHome bang_bang humidity scope: `bang_bang/bang_bang_climate.h` line 27 (sensor only, no setpoint/action)
- Related branches: `feat/humidity-control-tune` (10 s filter), water-interlock-removal branch.
