# beken-agora-mybot

BK7258 public POC for Agora MyBot style AI conversation.

This repository contains one product project:

- `projects/beken_agent_http`: AP+STA web provisioning, HTTP device binding, HTTP AI conversation start/stop, and RTC audio through `ai_iot_sdk`.

The repository intentionally excludes supplier-specific products such as R1 LCD/AVI and SpeedTech NFC/4G board code.

## Documents

- Install/build manual: [English](docs/INSTALL_BUILD_EN.md) / [中文](docs/INSTALL_BUILD_CN.md)
- Porting/debug manual: [English](docs/PORTING_AND_DEBUG_EN.md) / [中文](docs/PORTING_AND_DEBUG_CN.md)
- Public release scope: [English](docs/public-release-scope.md) / [中文](docs/PUBLIC_RELEASE_SCOPE_CN.md)

## Repository Layout

```text
ai_iot_sdk/                  # submodule: Agora AI IoT SDK
bk_avdk_smp/                 # submodule: official Beken BK7258 SDK
components/                  # product components needed by beken_agent_http
patches/beken/               # local Beken SDK patches carried by this POC
projects/beken_agent_http/   # BK7258 application project
tools/                       # patch/build helpers
```

## Quick Start

```bash
git clone --recurse-submodules https://github.com/harold-2022-cloud/beken-agora-mybot.git
cd beken-agora-mybot
tools/build_beken_agent_http.sh --clean
```

Copy firmware to the Windows burn tool folder:

```bash
tools/build_beken_agent_http.sh --clean --copy-to /path/to/beken-burn-tool
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

## Default Policy

- MQTT is disabled by default: `# CONFIG_HTTP_AGENT_ENABLE_MQTT is not set`.
- BLE provisioning is disabled by default: `# CONFIG_HTTP_AGENT_ENABLE_BLE_PROVISIONING is not set`.
- BK video engine/DVP camera is disabled by default: `# CONFIG_BK_VIDEO_ENGINE is not set`.
- RTC audio AIQOS is disabled in the product HTTP request and RTC token option.
- Product code owns Wi-Fi provisioning, HTTP binding, button policy, audio peripheral parameters, and optional AVI/LCD behavior.
- `ai_iot_sdk` owns RTC audio/video/datastream/private RTM backend and Agora vendor integration.

## Validation

Static guard:

```bash
python3 projects/beken_agent_http/tools/check_http_ai_product_boundary.py
```

Captive DNS after flashing:

```cmd
nslookup neverssl.com 192.168.4.1
```

Expected result: the A record resolves to `192.168.4.1`.

See the install/build manual and porting/debug manual above for full clone, build, burn, integration, and troubleshooting guidance.
