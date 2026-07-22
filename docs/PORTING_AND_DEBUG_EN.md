# Porting And Debug Manual

This document is for solution providers and secondary developers. It explains the product integration boundary and the main debug flow.

## Layering Principle

`ai_iot_sdk` owns:

- RTC facade public API.
- Agora RTC audio/video/datastream backend.
- Private RTM backend option.
- Agora vendor archive/header management.
- RTC lifecycle, join/leave/reconnect, audio/video/datastream send/receive.

The `beken_agent_http` product owns:

- Wi-Fi AP+STA provisioning.
- HTTP Device API: pair-code, binding-status, conversations/start, conversations/stop.
- Button behavior.
- Mic, speaker, codec, and audio gain.
- LCD/AVI display policy.
- Whether MQTT is enabled.
- Whether BLE is enabled.
- Flash key persistence policy.

Do not put GPIO, LCD, AVI, Wi-Fi, HTTP/MQTT topics, or BLE provisioning flow into the `ai_iot_sdk` public RTC API. They are product behavior.

## Main Files

Startup flow:

```text
projects/beken_agent_http/ap/ap_main.c
```

HTTP Device API:

```text
projects/beken_agent_http/ap/device_api_client.c
projects/beken_agent_http/ap/device_api_client.h
```

AP+STA web provisioning:

```text
projects/beken_agent_http/ap/http_wifi_provision_server.c
projects/beken_agent_http/ap/http_wifi_provision_server.h
```

Product workflow:

```text
projects/beken_agent_http/ap/entity_iot_process.c
projects/beken_agent_http/ap/entity_iot_process.h
```

RTC facade bridge:

```text
projects/beken_agent_http/ap/app_rtc_facade_bridge.c
projects/beken_agent_http/ap/app_rtc_facade_bridge.h
```

Audio parameters:

```text
projects/beken_agent_http/ap/audio_para.c
components/audio_engine/
```

AVI / LCD:

```text
components/bk_dual_screen_avi_player/
```

## HTTP Device API

Default endpoint:

```c
#define DEVICE_API_DEFAULT_BASE_URL "http://mybot.sg3.agoralab.co/api"
```

The default device id is derived from the Wi-Fi MAC:

```text
AG-XXXXXXXXXXXX
```

`Device_Api_Conversation_Start()` builds the AI start JSON body. This POC uses:

```json
{"features":{"ai_qos":false,"fast_send_multiplier":3,"show_transcript":true}}
```

## AP+STA Provisioning

`http_wifi_provision_server.c` owns:

- `/scan`: returns Wi-Fi SSID list.
- `/submit`: receives SSID/password.
- `/pair-code`: requests pair code only after STA has internet access.
- `/clear-env`: clears Wi-Fi/token development data.
- Captive redirect to `http://192.168.4.1/`.

Rules:

- Do not request pair code before Wi-Fi STA is connected.
- Do not print Wi-Fi passwords to logs.
- Avoid sleep/deepsleep during provisioning.

## RTC Flow

Full AI start flow:

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

`AI_RTC_FACADE_EVENT_REMOTE_USER_JOINED` means the remote AI user has joined the channel. The product should not treat mic uplink as active conversation audio before this event.

## Datastream / RTM / Emotion Payload

Product code should not depend directly on Agora RTM or datastream APIs.

The product receives RTC facade datastream callbacks:

```text
on_datastream_rx
```

For AI emotion data, use a stable JSON payload:

```json
{"type":"emotion","value":"happy","confidence":0.92}
```

The product parses it in `on_rtc_datastream_rx()` and maps it to UI or expression events:

```text
emotion=happy -> eye animation happy
emotion=thinking -> eye animation thinking
emotion=speaking -> eye animation speaking
```

## Debug Markers

Check these markers first:

```text
[HTTP_WIFI_PROV]      AP provisioning and pair-code page
[HTTP_DEVICE_API]     HTTP Device API
[PRODUCT_WORK]        product worker start/stop
[RTC_FACADE]          RTC facade state/event
[AI_RTC_AGORA]        Agora backend join/leave/callback
[RTC_FACADE_TX]       mic uplink
[RTC_FACADE_RX]       speaker downlink
[RTC_FACADE_DS]       datastream receive
```

How to classify failures:

- No `[HTTP_DEVICE_API] conversation started`: HTTP API did not return RTC token.
- No `[AI_RTC_AGORA] join`: token was not passed into the RTC facade.
- No `event=1 state=4`: RTC join did not complete.
- No `event=4 state=4`: remote AI user did not join the channel.
- RX exists but TX does not: check mic/audio read callback or audio TX gate.
- TX/RX both exist but have second-level gaps: usually product task, AVI/LCD, audio driver, or system resource contention.

## Performance Guidance

Use logs to locate the bottleneck before changing constants.

Recommended priority order during AI conversation:

1. Audio capture/AEC/encoder.
2. RTC send/receive.
3. Speaker playback.
4. UI/LCD/AVI.
5. Low-priority report/diagnostic work.

If AI audio disappears during conversation:

- Disable `CONFIG_BK_VIDEO_ENGINE` first to confirm DVP camera is not running.
- Disable `CONFIG_DUAL_SCREEN_AVI_PLAYER` next to confirm whether AVI/LCD causes TX/RX gaps.
- If AVI must stay enabled, reduce FPS, reduce JPEG size, lower AVI task priority, and refresh heavily only outside speech periods.

## Release Hygiene

Before publishing:

```bash
git status --short
rg -n -i "sentino|ct01|444AD6|password =|rtcToken|device_token" . \
  --glob '!**/.git/**' \
  --glob '!**/build/**' \
  --glob '!ai_iot_sdk/**' \
  --glob '!bk_avdk_smp/**'
python3 projects/beken_agent_http/tools/check_http_ai_product_boundary.py
```

Placeholder field names and documentation examples are allowed. Real UUID, SECRET, MAC, RTC token, MQTT password, and HTTP device token values are not allowed.
