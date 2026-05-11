# Ethernet (W5500) Support Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a compile-time-selectable ethernet variant to the OvoEasy firmware, allowing W5500-equipped units to run an ethernet-only network stack while the rest of the fleet continues to use WiFi — all from one source tree.

**Architecture:** Two firmware variants from a single source tree. A new `packages/base/network-ethernet.yaml` defines the W5500 stack; `ovoeasy.yaml` uses a `network_type` substitution to swap which network package is included. Default substitution is `wifi`; pass `-s network_type ethernet` at compile time for the ethernet variant. The two stacks are mutually exclusive (ESPHome rejects both `wifi:` and `ethernet:` blocks in one config), so failover/fallback is handled operationally — not at runtime.

**Tech Stack:** ESPHome 2025.12.x, ESP-IDF framework, ESP32-S3-WROOM-1U, W5500 SPI ethernet (dedicated SPI on GPIO35/36/37 + CS=39, RST=15).

**Spec:** `docs/superpowers/plans/2026-05-08-ethernet-failover-design.md`

---

## File Structure

| Path | Action | Responsibility |
|---|---|---|
| `packages/base/network-ethernet.yaml` | CREATE | W5500 ethernet config + `ethernet_info` text_sensor diagnostic |
| `ovoeasy.yaml` | MODIFY | Add `substitutions.network_type` + change network include path to use `${network_type}` |
| `CLAUDE.md` | MODIFY | Document variant build commands + fleet roster table |

The wifi package (`packages/base/network-wifi.yaml`) is **NOT** modified — it stays as it is today.

---

## Pre-Flight Check

Run once at the start of execution to make sure the conda env is ready and the baseline config is healthy.

- [ ] **Step 0a: Activate ESPHome conda env**

Run:
```bash
source /opt/miniconda3/etc/profile.d/conda.sh && conda activate esphome
esphome version
```
Expected: prints `Version: 2025.12.x` (any 2025.12 patch is fine).

- [ ] **Step 0b: Confirm baseline config validates**

Run:
```bash
esphome config ovoeasy.yaml
```
Expected: validation passes (no errors). This is the regression baseline — wifi variant must keep validating after our changes.

---

### Task 1: Create the ethernet package

**Files:**
- Create: `packages/base/network-ethernet.yaml`

- [ ] **Step 1: Create `packages/base/network-ethernet.yaml` with the exact contents below**

```yaml
# W5500 SPI ethernet — used by ethernet-variant builds only.
# Selected via `-s network_type ethernet` at compile time.
# See docs/superpowers/plans/2026-05-08-ethernet-failover-design.md for rationale.

ethernet:
  type: W5500
  clk_pin: GPIO36
  mosi_pin: GPIO35
  miso_pin: GPIO37
  cs_pin: GPIO39
  reset_pin: GPIO15
  polling_interval: 1s   # No INT pin wired. Explicit polling_interval closes
                         # the uninitialized-interrupt_pin footgun (esphome/issues#6268).

text_sensor:
  - platform: ethernet_info
    ip_address:
      name: "Ethernet IP"
      entity_category: diagnostic
```

- [ ] **Step 2: Verify the file does not affect the default (wifi) build yet**

The new file is not yet referenced from `ovoeasy.yaml`, so the wifi build must still pass.

Run:
```bash
esphome config ovoeasy.yaml
```
Expected: validation passes (same as baseline).

- [ ] **Step 3: Do NOT commit yet**

This change is staged for Task 2's commit, since the new file is only meaningful once `ovoeasy.yaml` wires it up.

---

### Task 2: Wire up the substitution in `ovoeasy.yaml`

**Files:**
- Modify: `ovoeasy.yaml` (add substitutions block at top, change network include line)

- [ ] **Step 1: Read current `ovoeasy.yaml`**

The current file starts with a `packages:` block and has no `substitutions:`. Line 7 reads:
```yaml
  network: !include packages/base/network-wifi.yaml
```

- [ ] **Step 2: Add substitutions block above `packages:`**

Insert these lines immediately before the `packages:` block (around current line 4):

```yaml
substitutions:
  # Network variant: "wifi" (default) or "ethernet".
  # Override per unit at compile time: `esphome run ovoeasy.yaml -s network_type ethernet`.
  # See docs/superpowers/plans/2026-05-08-ethernet-failover-design.md.
  network_type: wifi

```

- [ ] **Step 3: Change the network include line to use the substitution**

Replace the line:
```yaml
  network: !include packages/base/network-wifi.yaml
```
With:
```yaml
  network: !include packages/base/network-${network_type}.yaml
```

- [ ] **Step 4: Validate the wifi variant (regression check)**

Run:
```bash
esphome config ovoeasy.yaml
```
Expected: validation passes. The substitution defaults to `wifi`, so this should be equivalent to the baseline.

If this fails because ESPHome's substitution mechanism does not expand inside `!include` paths in this version, abort and apply the fallback (see Contingency below) before proceeding.

- [ ] **Step 5: Validate the ethernet variant**

Run:
```bash
esphome config ovoeasy.yaml -s network_type ethernet
```
Expected: validation passes. No "Component ethernet cannot be used together with component wifi" error (it must not — the wifi package is excluded by the substitution).

Watch the output for the resolved `ethernet:` block with the W5500 pins.

- [ ] **Step 6: Compile the wifi variant**

Run:
```bash
esphome compile ovoeasy.yaml
```
Expected: clean build, ends with `INFO Successfully compiled program`.

- [ ] **Step 7: Compile the ethernet variant**

Run:
```bash
esphome compile ovoeasy.yaml -s network_type ethernet
```
Expected: clean build, ends with `INFO Successfully compiled program`. Watch for any warnings about `ethernet_info` text_sensor or the W5500 driver.

If compile fails on the ethernet variant with a schema error on `polling_interval`, try removing that line — ESPHome's exact default-handling behavior for `polling_interval` has shifted across versions and the field may not be required at all. (If you remove it, leave a comment explaining the reason and reference issue #6268.)

- [ ] **Step 8: Commit Task 1 + Task 2 together**

```bash
git add packages/base/network-ethernet.yaml ovoeasy.yaml
git commit -m "feat: add ethernet (W5500) variant via network_type substitution"
```

#### Contingency (Step 4 fallback): substitution does not expand in `!include`

If `esphome config ovoeasy.yaml` fails after Step 3 with an error like `unable to read file packages/base/network-${network_type}.yaml`, ESPHome's substitution mechanism is not expanding inside the `!include` path. In that case:

1. Revert the change from Step 3 (restore the literal `network-wifi.yaml` line).
2. Create a new top-level entry point `ovoeasy-eth.yaml` that duplicates the package list of `ovoeasy.yaml` with one line different:
   - Replace `network: !include packages/base/network-wifi.yaml` with `network: !include packages/base/network-ethernet.yaml`.
   - Keep the `substitutions:` block out of this fallback path.
3. Build commands become `esphome run ovoeasy.yaml` (wifi) and `esphome run ovoeasy-eth.yaml` (ethernet).
4. Update Task 3's CLAUDE.md edit accordingly (two entry points instead of one with `-s`).

Re-run Steps 4–7 with the appropriate command for whichever entry point you're validating.

---

### Task 3: Update `CLAUDE.md` with build commands and fleet roster

**Files:**
- Modify: `CLAUDE.md` (extend "Build Commands" section + add "Firmware Variants" section)

- [ ] **Step 1: Replace the "Build Commands" section**

Find this block in `CLAUDE.md` (lines 52–58):
```bash
source /opt/miniconda3/etc/profile.d/conda.sh && conda activate esphome
esphome config ovoeasy.yaml        # Validate
esphome compile ovoeasy.yaml       # Build
esphome upload ovoeasy.yaml        # Flash (USB or OTA)
```

Replace it with:
```bash
source /opt/miniconda3/etc/profile.d/conda.sh && conda activate esphome

# WiFi variant (default)
esphome config ovoeasy.yaml
esphome compile ovoeasy.yaml
esphome upload ovoeasy.yaml

# Ethernet variant (W5500-equipped units)
esphome config ovoeasy.yaml -s network_type ethernet
esphome compile ovoeasy.yaml -s network_type ethernet
esphome upload ovoeasy.yaml -s network_type ethernet
```

- [ ] **Step 2: Add a "Firmware Variants" section after the Build Commands section**

Insert this new section immediately after the (newly expanded) Build Commands block, before whatever section follows it:

```markdown
## Firmware Variants

Two firmware variants share one source tree, selected at compile time via the `network_type` substitution:

- **`wifi`** (default) — uses `packages/base/network-wifi.yaml`. WiFi client + AP fallback + captive portal.
- **`ethernet`** — uses `packages/base/network-ethernet.yaml`. W5500 SPI ethernet only, no WiFi. Pass `-s network_type ethernet` to `esphome` commands.

The two stacks are mutually exclusive: ESPHome rejects YAML containing both `wifi:` and `ethernet:` blocks. Per-unit choice is therefore made at flash time, not runtime. See `docs/superpowers/plans/2026-05-08-ethernet-failover-design.md` for the full rationale.

### Fleet Roster

Track which units use which variant. Update when a unit's hardware or location changes.

| MAC suffix | Variant  | Location / Notes |
|------------|----------|------------------|
| _example_  | wifi     | _lab bench A_    |
| _example_  | ethernet | _wired rack B_   |

### Switching a unit between variants

Requires a USB reflash. An ethernet-variant unit cannot be reached via WiFi for OTA, and vice versa.
```

- [ ] **Step 3: Validate config still passes after CLAUDE.md edit**

The CLAUDE.md edit doesn't touch firmware, but run a quick sanity check anyway:

```bash
esphome config ovoeasy.yaml
esphome config ovoeasy.yaml -s network_type ethernet
```
Expected: both pass.

- [ ] **Step 4: Commit**

```bash
git add CLAUDE.md
git commit -m "docs: document ethernet variant build commands and fleet roster"
```

---

## Final Verification

- [ ] **Step F1: Both variants validate cleanly**

```bash
esphome config ovoeasy.yaml
esphome config ovoeasy.yaml -s network_type ethernet
```
Expected: both pass with no errors.

- [ ] **Step F2: Both variants compile cleanly**

```bash
esphome compile ovoeasy.yaml
esphome compile ovoeasy.yaml -s network_type ethernet
```
Expected: both produce `INFO Successfully compiled program`.

- [ ] **Step F3: Inspect the compiled ethernet variant for the expected components**

The ethernet variant must show evidence of the W5500 driver in build output. Look for `esp_eth_mac_w5500` or `ethernet_component_esp32` references in compile logs.

If anything in F1–F3 fails, return to the task where the failure manifests and re-run that task's steps.

- [ ] **Step F4: Hardware verification (DEFERRED — user has no hardware right now)**

Mark this step done by adding a TODO line at the bottom of the spec doc:
```
TODO 2026-05-11: Verify on hardware:
  - W5500-equipped unit, ethernet variant: gets DHCP IP, "Ethernet IP" sensor populated, HA sees device
  - WiFi-only unit, ethernet variant: w5500_verify_id failure logged, unit stays offline (acceptable per spec)
  - WiFi-only unit, wifi variant: identical to today's behavior (regression check)
```
This is a paper acknowledgement that hardware tests are pending. Commit the spec update:
```bash
git add docs/superpowers/plans/2026-05-08-ethernet-failover-design.md
git commit -m "docs: note hardware verification still pending for ethernet variant"
```

---

## Notes for the executing engineer

- **Conda env**: ESPHome commands MUST run inside the `esphome` conda env. The header of every shell session needs `source /opt/miniconda3/etc/profile.d/conda.sh && conda activate esphome` (already in CLAUDE.md).
- **ESPHome versions move fast**: if a schema option in `network-ethernet.yaml` is rejected (e.g. `polling_interval` is no longer valid), check the [ESPHome ethernet component docs](https://esphome.io/components/ethernet/) for the current option names. The W5500 driver itself has been stable since ESPHome 2023.x.
- **Do NOT touch `packages/base/network-wifi.yaml`** in this work. The wifi variant must remain bit-identical in behavior.
- **No tests in the traditional sense**: ESPHome doesn't have a unit test harness. The "tests" here are `esphome config` (schema validation) and `esphome compile` (link-level validation). Hardware verification (Step F4) is the final integration test but is deferred per user's current access constraints.
- **Commit style**: project uses Conventional Commits (`feat:`, `docs:`, etc.). No co-author attribution (user has it disabled globally).
