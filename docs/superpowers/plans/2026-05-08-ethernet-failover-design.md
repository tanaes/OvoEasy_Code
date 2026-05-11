# Ethernet (W5500) Support — Design Spec

**Date:** 2026-05-08
**Status:** Implemented 2026-05-11. **Contingency path taken** — ESPHome substitutions do NOT expand inside `!include` paths in 2025.12.4, so the two variants ship as separate top-level YAMLs (`ovoeasy.yaml` for wifi, `ovoeasy-eth.yaml` for ethernet) rather than `-s network_type`. See CLAUDE.md > "Firmware Variants" for the live build commands.

## Goal

Add optional W5500 SPI ethernet support to the OvoEasy fleet. Some units have a W5500 module installed on dedicated SPI pins; others do not. Units with the module should use ethernet (radios off); units without should use WiFi as today.

## Why two firmware variants instead of runtime detection

The original goal — single firmware with runtime detection and failover — is not feasible in ESPHome 2025.12.x. The framework's config validator rejects YAML containing both `wifi:` and `ethernet:` blocks with the error *"Component ethernet cannot be used together with component wifi"*. This restriction is enforced at compile time and is independent of `enable_on_boot` or any runtime gating. Open feature requests
[esphome/feature-requests#2102](https://github.com/esphome/feature-requests/issues/2102)
and
[esphome/feature-requests#1357](https://github.com/esphome/feature-requests/issues/1357)
exist but are not implemented upstream.

Alternatives considered and rejected:
- **Custom external component** that bypasses ESPHome's `ethernet:` schema and drives `esp_eth` directly. ~2–3 days dev + ongoing maintenance against ESPHome internals. Rejected as over-engineered for ~20 units.
- **WiFi only, ignore the W5500.** Defers the problem indefinitely.
- **Patch ESPHome to lift the validator restriction.** Fights upstream; not a sustainable position.

The selected approach: **two firmware variants from a single source tree, selected by substitution at compile time.** Source code stays unified; per-unit binary selection is an operational concern handled by a small fleet roster.

## Architecture

### File changes

```
packages/base/
  network-wifi.yaml         (UNCHANGED — wifi + AP fallback + captive portal)
  network-ethernet.yaml     (NEW — W5500 + ethernet diagnostics)
ovoeasy.yaml                (MODIFIED — substitution-driven network include)
docs/fleet-roster.md        (NEW — MAC suffix → variant mapping)
```

### Substitution mechanism

In `ovoeasy.yaml`:

```yaml
substitutions:
  network_type: wifi    # default; override per unit at compile time

packages:
  ...
  network: !include packages/base/network-${network_type}.yaml
  ...
```

Compile commands:
```bash
esphome run ovoeasy.yaml                              # WiFi variant (default)
esphome run ovoeasy.yaml -s network_type ethernet     # Ethernet variant
```

If ESPHome's substitution mechanism does not expand inside `!include` paths in the installed version (verify on first compile), the fallback is a thin `ovoeasy-eth.yaml` wrapper that duplicates the package list with the network line swapped. Single substitution is preferred for DRY-ness.

### Ethernet package (`packages/base/network-ethernet.yaml`)

```yaml
ethernet:
  type: W5500
  clk_pin: GPIO36
  mosi_pin: GPIO35
  miso_pin: GPIO37
  cs_pin: GPIO39
  reset_pin: GPIO15
  polling_interval: 1s    # no INT pin wired; explicit setting closes the
                          # uninitialized-interrupt_pin footgun (esphome/issues#6268)

text_sensor:
  - platform: ethernet_info
    ip_address:
      name: "Ethernet IP"
      entity_category: diagnostic
```

Pin assignments are dictated by hardware:
- GPIO35 = MOSI
- GPIO36 = CLK
- GPIO37 = MISO
- GPIO39 = CS (per existing `ETH_CS` in CLAUDE.md pin map)
- GPIO15 = RST (per existing `ETH_RST` in CLAUDE.md pin map)
- No interrupt line connected on the PCB — polling mode used

These pins are entirely separate from the existing `spi:` bus on GPIO11/12/13 (display + SD card). ESPHome's W5500 ethernet driver manages its own SPI host internally; no contention with the shared SPI bus.

### WiFi package: unchanged

`packages/base/network-wifi.yaml` keeps its current contents — `wifi:` block, `ap:` fallback, `captive_portal:`, and `wifi_signal` sensor. The wifi variant of the firmware behaves identically to today.

## Behavior

| Scenario | WiFi variant | Ethernet variant |
|---|---|---|
| Configured network works | Connects normally | Connects normally |
| Network unreachable at boot | `OvoEasy Fallback` AP activates | Unit offline until network restored |
| W5500 chip absent on an ethernet-variant unit | N/A | `w5500_verify_id` mismatch logged, ethernet component marked failed, unit stays offline. Resolution: install hardware or reflash with wifi variant. |
| Wrong variant flashed | Discoverable: ethernet unit on a wifi LAN never gets IP, vice versa | Same |
| Cable unplugged after boot (eth variant) | N/A | Unit offline until cable restored |
| Per-unit identity in HA | `ovoeasy-XXXXXX` (MAC suffix) | Same — variant does not affect name |

## Operational workflow

### Fleet roster

Maintain a simple list at `docs/fleet-roster.md` mapping each unit's MAC suffix to its variant. Format suggestion:

```
| MAC suffix | Variant   | Notes                              |
|------------|-----------|------------------------------------|
| AABBCC     | wifi      |                                    |
| 112233     | ethernet  | Wired in lab, installed 2026-05-12 |
```

This list is the source of truth for which binary to flash to which unit.

### OTA pushes

```bash
# Wifi unit
esphome run ovoeasy.yaml --device ovoeasy-AABBCC.local

# Ethernet unit
esphome run ovoeasy.yaml -s network_type ethernet --device ovoeasy-112233.local
```

### Switching a unit between variants

Requires a USB reflash. Reasoning: an ethernet-variant unit with no wifi credentials cannot be reached over WiFi for OTA, so flipping it to the wifi variant requires direct USB. Same in reverse if wifi is misconfigured. Plan: keep a USB cable accessible and known-good wifi credentials on hand for emergencies.

## Diagnostics & visibility

- **Ethernet variant** exposes the `Ethernet IP` text sensor (entity_category: diagnostic). When the link is down or the chip is absent, the IP field is empty.
- **Wifi variant** continues to expose the existing `WiFi Signal`, `IP Address`, and `MAC Address` sensors via the `core.yaml` package.
- ESPHome boot logs identify the active stack and any W5500 init failure (visible via USB serial or HA log stream).

## Tradeoffs accepted

- **No automatic fallback** between ethernet and wifi on a single unit. The variant decision is binary and made at flash time.
- **Two binaries to track** at flash time. With ~20 units, manageable via the fleet roster.
- **Lost ethernet link = offline unit** for ethernet variants. Mitigation: USB reflash path always available.
- **Operational discipline required**: flashing the wrong variant to a unit produces an offline unit until corrected. No technical guardrail prevents this; the roster + clear OTA commands are the controls.

## Implementation notes

- ESPHome 2025.12.x. ESP-IDF framework. ESP32-S3.
- The W5500 driver in ESPHome wraps ESP-IDF's `esp_eth_mac_w5500` driver. Confirmed gracefully handles a missing chip: `w5500_verify_id` reads `VERSIONR` (0x0039), fails with `"W5500 version mismatched, expected 0x04, got 0xXX"` when SPI returns garbage, marks the component failed, boot continues. (Source: `esp_eth_mac_w5500.c` in IDF 5.x; ESPHome `ethernet_component_esp32.cpp` lines 69–73, 333–334.)
- `polling_interval: 1s` is set explicitly to avoid the uninitialized-`interrupt_pin_` panic documented in [esphome/issues#6268](https://github.com/esphome/issues/issues/6268). Polling at 1s is generous for our telemetry-grade traffic and keeps CPU cool.
- The `ethernet_info` text_sensor schema has been stable in ESPHome for years; no compatibility risk anticipated.

## Out of scope

- Hot-plug detection (cable inserted/removed after boot). Variant-locked design makes this a non-issue.
- Static IP assignment. DHCP suffices for the lab environment.
- VLAN tagging, 802.1X, or other enterprise networking features.
- Power-over-Ethernet (the W5500 module on this board is not POE-capable).
