# beken-agora-mybot

BK7258 public POC for Agora MyBot style AI conversation.

This repository contains one product project:

- `projects/beken_agent_http`: AP+STA web provisioning, HTTP device binding, HTTP AI conversation start/stop, and RTC audio through `ai_iot_sdk`.

The repository intentionally excludes supplier-specific products such as R1 LCD/AVI and SpeedTech NFC/4G board code.

## Repository Layout

```text
ai_iot_sdk/                  # submodule: Agora AI IoT SDK
bk_avdk_smp/                 # submodule: official Beken BK7258 SDK
components/                  # product components needed by beken_agent_http
patches/beken/               # local Beken SDK patches carried by this POC
projects/beken_agent_http/   # BK7258 application project
tools/                       # patch/build helpers
```

## Clone

```bash
git clone --recurse-submodules <this-repo-url>
cd beken-agora-mybot
```

If submodules were not cloned:

```bash
git submodule update --init --recursive
```

## Apply Beken SDK Patches

The Beken SDK is kept as an official submodule. Product-required SDK changes are stored as patches.

```bash
tools/apply_beken_patches.sh
```

Current patch:

- `patches/beken/2026-07-22-beken-easyflash-ap-env-16k.patch`
- `patches/beken/2026-07-22-beken-dhcpd-captive-dns-config.patch`

The EasyFlash patch expands AP ENV from 8K to 16K so Wi-Fi config and `http_device_token` can be persisted reliably.

The DHCP/DNS patch adds `CONFIG_BK_DHCPD_CAPTIVE_DNS`, default off. `beken_agent_http` enables it so captive portal DNS can answer A records with `192.168.4.1`.

## Build

Toolchain default:

```text
/opt/gcc-arm-none-eabi-10.3-2021.10/bin
```

Build:

```bash
tools/build_beken_agent_http.sh --clean
```

Build and copy to the Windows burn tool directory:

```bash
tools/build_beken_agent_http.sh --clean --copy-to /mnt/c/Users/harold.chen/Desktop/bk-burn-tool/BEKEN_BKFIL_V3.0.1.4_314_20240924
```

Output files:

```text
projects/beken_agent_http/build/bk7258/beken_agent_http/package/all-app.bin
projects/beken_agent_http/build/bk7258/beken_agent_http/package/app_pack.rbl
```

## Runtime Flow

1. Device starts AP+STA provisioning when Wi-Fi is not configured or reset provisioning is triggered.
2. User connects to the device AP.
3. Browser opens `http://192.168.4.1`.
4. User selects Wi-Fi SSID and submits password.
5. Device connects STA to the selected Wi-Fi.
6. Web page polls pair-code status.
7. Device calls MyBot HTTP Device API and shows `PAIR_CODE`.
8. After binding succeeds, device stores `http_device_token`.
9. AI button starts `/devices/{device_id}/conversations/start`.
10. RTC token result is passed to `ai_iot_sdk` RTC facade.

## Public Configuration Notes

`projects/beken_agent_http/ap/entity_iot_process.h` contains placeholder MQTT product/triple values. MQTT is disabled by default for this HTTP POC:

```text
# CONFIG_HTTP_AGENT_ENABLE_MQTT is not set
```

Replace placeholders only if you explicitly enable MQTT management.

The HTTP Device API default endpoint is in:

```text
projects/beken_agent_http/ap/device_api_client.c
```

## Validation

Static guard:

```bash
python3 projects/beken_agent_http/tools/check_http_ai_product_boundary.py
```

Captive DNS after flashing:

```cmd
nslookup neverssl.com 192.168.4.1
```

Expected result: the A record resolves to `192.168.4.1`, not `Query refused`.
