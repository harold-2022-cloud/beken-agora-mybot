# 公開範圍說明

這個 repository 是 BK7258 AP 網頁配網加 Agora MyBot HTTP AI 對話的公開 POC。

包含：

- `beken_agent_http` 產品項目。
- 這個項目需要的共用產品組件。
- `ai_iot_sdk` submodule。
- Beken 官方 `bk_avdk_smp` submodule。
- 產品自帶的 Beken SDK patch 檔。

不包含：

- R1 LCD/AVI 供應商產品。
- SpeedTech NFC/4G 產品。
- 工廠原理圖與供應商私有板級文件。
- 真實 UUID、SECRET、MAC、RTC token、MQTT password、device token。

Beken SDK patch 模型是刻意保留的：下游開發者可以從 Beken 官方 SDK 開始，只套用這個產品需要的 patch。

目前 Beken patch：

- `patches/beken/2026-07-22-beken-easyflash-ap-env-16k.patch`
  - AP EasyFlash ENV 從 8K 擴到 16K，用於保存 Wi-Fi config 和 HTTP device token。
- `patches/beken/2026-07-22-beken-dhcpd-captive-dns-config.patch`
  - 新增預設關閉的 captive DNS 支援，AP 網頁配網期間可讓 DNS A record 回 `192.168.4.1`。
