# 二次開發與 Debug 手冊

本文檔給方案商或二次開發者使用，說明產品層如何接入與調試。

## 分層原則

`ai_iot_sdk` 負責：

- RTC facade public API。
- Agora RTC audio/video/datastream backend。
- private RTM backend option。
- Agora vendor archive/header 管理。
- RTC lifecycle、join/leave/reconnect、audio/video/datastream send/receive。

`beken_agent_http` product 負責：

- Wi-Fi AP+STA 配網。
- HTTP Device API：pair-code、binding-status、conversations/start、conversations/stop。
- button 行為。
- mic/speaker/codec/audio gain。
- LCD/AVI 顯示策略。
- MQTT 是否啟用。
- BLE 是否啟用。
- flash key 保存策略。

不要把 GPIO、LCD、AVI、Wi-Fi、HTTP/MQTT topic、BLE 配網流程塞進 `ai_iot_sdk` public RTC API。這些是產品行為。

## 主要檔案

啟動流程：

```text
projects/beken_agent_http/ap/ap_main.c
```

HTTP Device API：

```text
projects/beken_agent_http/ap/device_api_client.c
projects/beken_agent_http/ap/device_api_client.h
```

AP+STA 網頁配網：

```text
projects/beken_agent_http/ap/http_wifi_provision_server.c
projects/beken_agent_http/ap/http_wifi_provision_server.h
```

Product 工作流：

```text
projects/beken_agent_http/ap/entity_iot_process.c
projects/beken_agent_http/ap/entity_iot_process.h
```

RTC facade bridge：

```text
projects/beken_agent_http/ap/app_rtc_facade_bridge.c
projects/beken_agent_http/ap/app_rtc_facade_bridge.h
```

Audio 參數：

```text
projects/beken_agent_http/ap/audio_para.c
components/audio_engine/
```

AVI / LCD：

```text
components/bk_dual_screen_avi_player/
```

## HTTP Device API

預設 endpoint：

```c
#define DEVICE_API_DEFAULT_BASE_URL "http://mybot.sg3.agoralab.co/api"
```

device id 預設用 Wi-Fi MAC 組成：

```text
AG-XXXXXXXXXXXX
```

AI start request 的 features 在 `Device_Api_Conversation_Start()` 裡組 JSON。此 POC 預設：

```json
{"features":{"ai_qos":false,"fast_send_multiplier":3,"show_transcript":true}}
```

## AP+STA 配網

`http_wifi_provision_server.c` 負責：

- `/scan` 回 Wi-Fi SSID list。
- `/submit` 接收 SSID/password。
- `/pair-code` 在 STA 連上 internet 後才請求 pair code。
- `/clear-env` 清除 Wi-Fi/token 開發資料。
- captive redirect 到 `http://192.168.4.1/`。

注意：

- Wi-Fi STA 未連上前，不應真的請求 pair code。
- password 不應打印到 log。
- 配網期間應避免進入 sleep/deepsleep。

## RTC 使用流程

完整 AI start：

```text
button
  -> Product_Http_Ai_Start()
  -> Device_Api_Conversation_Start()
  -> parse app_id/channel/uid/user_account/token
  -> App_Rtc_Facade_Bridge_On_Token_Result_With_User_Account()
  -> Ai_Rtc_Facade_On_Token_Result()
  -> Agora join
  -> AI_RTC_FACADE_EVENT_JOINED
  -> AI_RTC_FACADE_EVENT_REMOTE_USER_JOINED
  -> audio read callback sends frames through Ai_Rtc_Facade_Send_Audio()
```

`AI_RTC_FACADE_EVENT_REMOTE_USER_JOINED` 表示遠端 AI user 已進 channel。產品層應避免在 remote joined 前把 mic audio 當成有效對話送出。

## Datastream / RTM / 表情資訊

上層產品不要直接依賴 Agora RTM/datastream API。

產品看到的是 RTC facade datastream callback：

```text
on_datastream_rx
```

如果 AI 雲端下發情緒資訊，建議 payload 用穩定 JSON：

```json
{"type":"emotion","value":"happy","confidence":0.92}
```

產品層在 `on_rtc_datastream_rx()` parse 後轉成自己的 UI/表情事件：

```text
emotion=happy -> eye animation happy
emotion=thinking -> eye animation thinking
emotion=speaking -> eye animation speaking
```

## Debug marker

優先看：

```text
[HTTP_WIFI_PROV]      AP 配網與 pair-code 網頁
[HTTP_DEVICE_API]     HTTP Device API
[PRODUCT_WORK]        product worker start/stop
[RTC_FACADE]          RTC facade state/event
[AI_RTC_AGORA]        Agora backend join/leave/callback
[RTC_FACADE_TX]       mic uplink
[RTC_FACADE_RX]       speaker downlink
[RTC_FACADE_DS]       datastream receive
```

判斷卡點：

- 沒有 `[HTTP_DEVICE_API] conversation started`：卡在 HTTP API。
- 沒有 `[AI_RTC_AGORA] join`：token 沒交給 RTC facade。
- 沒有 `event=1 state=4`：RTC join 沒成功。
- 沒有 `event=4 state=4`：remote AI user 沒進 channel。
- 有 RX 沒 TX：mic/audio read callback 或 audio TX gate 問題。
- TX/RX 都有但中間秒級 gap：通常是 product task、AVI/LCD、audio driver 或系統資源競爭。

## 性能調整建議

先用 log 定位，不要直接調數字。

音訊對話優先順序：

1. audio capture/AEC/encoder。
2. RTC send/receive。
3. speaker playback。
4. UI/LCD/AVI。
5. 低優先級 report/diagnostic。

如果 AI 對話中間消失：

- 先關 `CONFIG_BK_VIDEO_ENGINE`，確認 DVP camera 沒有跑。
- 再測關 `CONFIG_DUAL_SCREEN_AVI_PLAYER`，確認 AVI/LCD 是否造成 TX/RX gap。
- AVI 必須保留時，降低 FPS、降低 JPEG 尺寸、降低 AVI task priority，並只在非說話期間高頻刷新。

## Release hygiene

公開前檢查：

```bash
git status --short
rg -n -i "sentino|ct01|444AD6|password =|rtcToken|device_token" . \
  --glob '!**/.git/**' \
  --glob '!**/build/**' \
  --glob '!ai_iot_sdk/**' \
  --glob '!bk_avdk_smp/**'
python3 projects/beken_agent_http/tools/check_http_ai_product_boundary.py
```

允許出現 placeholder、欄位名稱、文件說明。不允許出現真實 UUID、SECRET、MAC、RTC token、MQTT password、HTTP device token。
