# OTA Updates

After the initial USB flash, firmware updates can be delivered wirelessly over the local network (OTA — Over The Air). This is the standard update method for deployed units.

## Prerequisites

- Device is powered on and connected to WiFi
- Your computer is on the same network as the device
- You know the device's IP address or mDNS name (see [Network Setup](04-network.md))

## Steps

### 1. Upload over the network

```bash
source /opt/miniconda3/etc/profile.d/conda.sh && conda activate esphome
esphome upload ovoeasy.yaml --device ovoeasy-XXXX.local
```

Replace `ovoeasy-XXXX.local` with the device's mDNS name, or use its IP address directly:

```bash
esphome upload ovoeasy.yaml --device 192.168.x.x
```

The upload takes 20–40 seconds. The device reboots automatically when complete.

### 2. Verify

Check the device came back online:

```bash
esphome logs ovoeasy.yaml --device ovoeasy-XXXX.local
```

Or open the web interface at `http://ovoeasy-XXXX.local` and confirm it loads.

## Fallback to USB

If OTA fails (device unreachable, crash loop, wrong IP), fall back to USB:

1. Connect USB-C cable
2. Follow the [Initial Flashing](01-flashing.md) procedure

## Gotchas

> ⚠️ **Same network required**
> Your computer and the device must be on the same subnet. OTA will time out if separated by a router that blocks mDNS or local broadcasts.

> ⚠️ **Device in crash loop**
> If the device is rebooting repeatedly, OTA will not succeed. Connect via USB, run `esphome logs`, and diagnose the error before retrying OTA.

> ⚠️ **reboot_timeout is disabled**
> The device is configured with `reboot_timeout: 0s` — it will not automatically reboot if it loses network or Home Assistant connectivity. A device stuck mid-update requires a manual power cycle.
