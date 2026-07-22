# Installation And Build Manual

This document explains how to build `beken-agora-mybot` from a clean GitHub clone.

## Project Scope

`beken-agora-mybot` is a public BK7258 POC:

- AP+STA web provisioning, without BLE dependency.
- HTTP Device API for device binding and AI conversation start/stop.
- RTC voice conversation through the `ai_iot_sdk` facade.
- MQTT is disabled by default.
- DVP `CONFIG_BK_VIDEO_ENGINE` is disabled by default.

## Directory Layout

```text
ai_iot_sdk/                  # submodule: Agora AI IoT SDK
bk_avdk_smp/                 # submodule: official Beken SDK release/v3.1.1
components/                  # product components used by the POC
patches/beken/               # product patches applied to the Beken SDK
projects/beken_agent_http/   # BK7258 POC app
tools/                       # build and patch helpers
```

## Windows / WSL Environment

Build from WSL Ubuntu. Use Windows for the Beken burn tool.

From Windows CMD:

```cmd
wsl
```

In WSL, verify the toolchain:

```bash
ls -l /opt/gcc-arm-none-eabi-10.3-2021.10/bin/arm-none-eabi-gcc
```

Default toolchain path:

```text
/opt/gcc-arm-none-eabi-10.3-2021.10/bin
```

Override it if needed:

```bash
export COMPILER_TOOLCHAIN_PATH=/your/toolchain/path/bin
```

## Clone

```bash
git clone --recurse-submodules https://github.com/harold-2022-cloud/beken-agora-mybot.git
cd beken-agora-mybot
```

If submodules were not cloned:

```bash
git submodule update --init --recursive
```

Verify submodules:

```bash
git submodule status
```

## Beken SDK Patches

`bk_avdk_smp` is kept as an official submodule. Product-required Beken SDK changes are stored under:

```text
patches/beken/
```

Current patches:

```text
2026-07-22-beken-easyflash-ap-env-16k.patch
2026-07-22-beken-dhcpd-captive-dns-config.patch
```

Purpose:

- Expand AP EasyFlash ENV from 8K to 16K so Wi-Fi config and `http_device_token` can be persisted.
- Add `CONFIG_BK_DHCPD_CAPTIVE_DNS` so AP provisioning can resolve DNS A records to `192.168.4.1`.

The build script applies patches automatically by default:

```bash
tools/build_beken_agent_http.sh --clean
```

Manual patch command:

```bash
tools/apply_beken_patches.sh
```

After patches are applied, the `bk_avdk_smp` submodule worktree becomes dirty. This is expected. To restore the submodule before release:

```bash
git -C bk_avdk_smp restore ap/components/easy_flash/easy_flash_V4.X/inc/ef_cfg.h
git -C bk_avdk_smp restore ap/components/lwip_intf_v2_1/Kconfig
git -C bk_avdk_smp restore ap/components/lwip_intf_v2_1/dhcpd/dhcp-server.c
git -C bk_avdk_smp restore cp/components/lwip_intf_v2_1/Kconfig
git -C bk_avdk_smp restore cp/components/lwip_intf_v2_1/dhcpd/dhcp-server.c
```

## Build

```bash
tools/build_beken_agent_http.sh --clean
```

Output files:

```text
projects/beken_agent_http/build/bk7258/beken_agent_http/package/all-app.bin
projects/beken_agent_http/build/bk7258/beken_agent_http/package/app_pack.rbl
```

Copy firmware to the Windows burn tool folder:

```bash
tools/build_beken_agent_http.sh --clean --copy-to /path/to/beken-burn-tool
```

Copied filenames:

```text
beken_agent_http_mybot_all-app.bin
beken_agent_http_mybot_app_pack.rbl
```

If patches are already applied manually, skip automatic patching:

```bash
tools/build_beken_agent_http.sh --clean --no-apply-patches
```

## Flashing And Console

Flash this file with the Windows Beken burn tool:

```text
beken_agent_http_mybot_all-app.bin
```

Common console setting:

```text
UART0 GPIO11/GPIO10 @115200
```

## AP Provisioning Validation

When Wi-Fi is missing or reprovisioning is triggered, the device starts AP mode.

Connect a phone or PC to the device AP, then open:

```text
http://192.168.4.1
```

Captive DNS validation from Windows CMD:

```cmd
nslookup neverssl.com 192.168.4.1
```

Expected result: the A record resolves to `192.168.4.1`. If the command returns `Query refused`, the Beken SDK patch or `CONFIG_BK_DHCPD_CAPTIVE_DNS` is not active.

## AI Conversation Validation

Expected runtime markers:

```text
[HTTP_DEVICE_API] pair-code request device=AG-...
[HTTP_WIFI_PROV] pair-code ready code=...
[HTTP_DEVICE_API] device bound
[PRODUCT_WORK] ai start via HTTP
[HTTP_DEVICE_API] conversation start request
[HTTP_DEVICE_API] conversation started id=...
[RTC_FACADE] token result options ai_qos=0
[AI_RTC_AGORA] join conn=...
[RTC_FACADE] event=1 state=4
[RTC_FACADE] event=4 state=4
[RTC_FACADE_TX] frame=...
[RTC_FACADE_RX] frame=...
```

This POC disables AIQOS by default, so this marker should show:

```text
[RTC_FACADE] token result options ai_qos=0
```

## FAQ

Submodule missing:

```bash
git submodule update --init --recursive
```

Compiler missing:

```bash
echo $COMPILER_TOOLCHAIN_PATH
ls -l /opt/gcc-arm-none-eabi-10.3-2021.10/bin/arm-none-eabi-gcc
```

Flash persistence failure:

```text
ENV start address ... size is 16384 bytes
```

Wi-Fi connected but pair code does not refresh:

```text
[HTTP_WIFI_PROV] phase=wifi_connected
[HTTP_DEVICE_API] pair-code request
```
