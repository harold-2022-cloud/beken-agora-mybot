# Public Release Scope

This repository is a public POC for BK7258 AP web provisioning plus Agora MyBot HTTP AI conversation.

Included:

- `beken_agent_http` product project.
- Shared product components required by that project.
- `ai_iot_sdk` as a submodule.
- Beken official `bk_avdk_smp` as a submodule.
- Product-owned patch files for Beken SDK changes.

Excluded:

- R1 LCD/AVI supplier product.
- SpeedTech NFC/4G product.
- Factory schematic and vendor-private board documents.
- Real UUID, secret, MAC, RTC token, MQTT password, and device token values.

The Beken SDK patch model is intentional: downstream users can pull the official Beken SDK and apply only the product patches they need.

Current Beken patches:

- `patches/beken/2026-07-22-beken-easyflash-ap-env-16k.patch`
  - Expands AP EasyFlash ENV from 8K to 16K for Wi-Fi config and HTTP device token persistence.
- `patches/beken/2026-07-22-beken-dhcpd-captive-dns-config.patch`
  - Adds default-off captive DNS support for AP web provisioning.
