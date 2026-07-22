# 安裝與編譯手冊

本文檔說明如何從乾淨的 GitHub clone 編譯 `beken-agora-mybot`。

## 專案定位

`beken-agora-mybot` 是 BK7258 公開 POC：

- AP+STA 網頁配網，不依賴 BLE。
- HTTP Device API 完成設備綁定與 AI 對話 start/stop。
- RTC 語音通話使用 `ai_iot_sdk` facade。
- MQTT 預設關閉。
- DVP `CONFIG_BK_VIDEO_ENGINE` 預設關閉。

## 目錄結構

```text
ai_iot_sdk/                  # submodule，Agora AI IoT SDK
bk_avdk_smp/                 # submodule，Beken 官方 SDK release/v3.1.1
components/                  # POC 使用的產品組件
patches/beken/               # 對 Beken SDK 的產品補丁
projects/beken_agent_http/   # BK7258 POC app
tools/                       # 編譯與 patch 工具
```

## Windows / WSL 環境

建議在 WSL Ubuntu 裡編譯，Windows 只負責燒錄工具。

Windows CMD：

```cmd
wsl
```

WSL 裡確認工具鍊：

```bash
ls -l /opt/gcc-arm-none-eabi-10.3-2021.10/bin/arm-none-eabi-gcc
```

預設工具鍊路徑：

```text
/opt/gcc-arm-none-eabi-10.3-2021.10/bin
```

如需覆蓋：

```bash
export COMPILER_TOOLCHAIN_PATH=/your/toolchain/path/bin
```

## Clone

```bash
git clone --recurse-submodules https://github.com/harold-2022-cloud/beken-agora-mybot.git
cd beken-agora-mybot
```

如果 clone 時沒有帶 submodule：

```bash
git submodule update --init --recursive
```

確認：

```bash
git submodule status
```

## Beken SDK Patch

`bk_avdk_smp` 保持為官方 submodule。POC 需要的 Beken SDK 修改放在：

```text
patches/beken/
```

目前 patch：

```text
2026-07-22-beken-easyflash-ap-env-16k.patch
2026-07-22-beken-dhcpd-captive-dns-config.patch
```

用途：

- EasyFlash AP ENV 從 8K 調到 16K，避免 Wi-Fi config 與 `http_device_token` 寫入空間不足。
- DHCP captive DNS 加 `CONFIG_BK_DHCPD_CAPTIVE_DNS`，AP 配網期間 DNS A record 回 `192.168.4.1`。

build script 預設會自動套用 patch：

```bash
tools/build_beken_agent_http.sh --clean
```

也可以手動套用：

```bash
tools/apply_beken_patches.sh
```

patch 套用後，`bk_avdk_smp` submodule 工作區會變 dirty，這是預期現象。交付前如要還原 submodule：

```bash
git -C bk_avdk_smp restore ap/components/easy_flash/easy_flash_V4.X/inc/ef_cfg.h
git -C bk_avdk_smp restore ap/components/lwip_intf_v2_1/Kconfig
git -C bk_avdk_smp restore ap/components/lwip_intf_v2_1/dhcpd/dhcp-server.c
git -C bk_avdk_smp restore cp/components/lwip_intf_v2_1/Kconfig
git -C bk_avdk_smp restore cp/components/lwip_intf_v2_1/dhcpd/dhcp-server.c
```

## 編譯

```bash
tools/build_beken_agent_http.sh --clean
```

輸出：

```text
projects/beken_agent_http/build/bk7258/beken_agent_http/package/all-app.bin
projects/beken_agent_http/build/bk7258/beken_agent_http/package/app_pack.rbl
```

複製到 Windows 燒錄工具資料夾：

```bash
tools/build_beken_agent_http.sh --clean --copy-to /path/to/beken-burn-tool
```

複製後檔名：

```text
beken_agent_http_mybot_all-app.bin
beken_agent_http_mybot_app_pack.rbl
```

如果已手動套好 patch，可跳過自動 patch 檢查：

```bash
tools/build_beken_agent_http.sh --clean --no-apply-patches
```

## 燒錄與 Console

使用 Windows Beken 燒錄工具燒：

```text
beken_agent_http_mybot_all-app.bin
```

常用 console：

```text
UART0 GPIO11/GPIO10 @115200
```

## AP 配網驗證

沒有 Wi-Fi 或觸發重新配網時，設備會開 AP。

手機或 PC 連上設備 AP 後打開：

```text
http://192.168.4.1
```

Captive DNS 驗證：

```cmd
nslookup neverssl.com 192.168.4.1
```

預期回 `192.168.4.1`。如果看到 `Query refused`，表示 Beken SDK patch 或 `CONFIG_BK_DHCPD_CAPTIVE_DNS` 沒有生效。

## AI 對話驗證

正常流程 log：

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

這個 POC 預設 AIQOS 關閉，所以應看到：

```text
[RTC_FACADE] token result options ai_qos=0
```

## 常見問題

找不到 submodule：

```bash
git submodule update --init --recursive
```

找不到 compiler：

```bash
echo $COMPILER_TOOLCHAIN_PATH
ls -l /opt/gcc-arm-none-eabi-10.3-2021.10/bin/arm-none-eabi-gcc
```

Flash 空間不足或 token 寫不進去：

```text
ENV start address ... size is 16384 bytes
```

Wi-Fi 已連上但 pair code 不刷新：

```text
[HTTP_WIFI_PROV] phase=wifi_connected
[HTTP_DEVICE_API] pair-code request
```
