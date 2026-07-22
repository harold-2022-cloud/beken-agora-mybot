#!/usr/bin/env python3
"""Static checks for Phase 7 W7 BK RTC facade boundaries."""

from __future__ import annotations

import re
from pathlib import Path


ROOT = Path("/root/smp/bk_solution_ai")
PROJECT = ROOT / "projects/beken_genie_rino"


def index_or_fail(text: str, needle: str, label: str) -> int:
    index = text.find(needle)
    if index < 0:
        raise AssertionError(f"missing {label}: {needle}")
    return index


def main() -> int:
    failures: list[str] = []

    audio_c = (ROOT / "components/audio_engine/audio_engine.c").read_text(encoding="utf-8")
    audio_h = (ROOT / "components/audio_engine/audio_engine.h").read_text(encoding="utf-8")
    ap_main = (PROJECT / "ap/ap_main.c").read_text(encoding="utf-8")
    bridge_c = (PROJECT / "ap/app_rtc_facade_bridge.c").read_text(encoding="utf-8")
    bridge_h = (PROJECT / "ap/app_rtc_facade_bridge.h").read_text(encoding="utf-8")
    entity_c = (PROJECT / "ap/entity_iot_process.c").read_text(encoding="utf-8")

    if '#include "network_transfer.h"' in audio_c:
        failures.append("audio_engine.c still includes network_transfer.h")
    if re.search(r"\bntwk_trans_send_audio\s*\(", audio_c):
        failures.append("audio_engine.c still calls ntwk_trans_send_audio")
    if "audio_engine_init_with_read_callback" not in audio_h:
        failures.append("audio_engine.h missing audio_engine_init_with_read_callback")
    if "App_Rtc_Facade_Bridge_Audio_Read_Callback" not in bridge_h:
        failures.append("bridge header missing audio read callback")
    if "App_Rtc_Facade_Bridge_Set_Token_Request_Callback" not in bridge_h:
        failures.append("bridge header missing token request callback setter")
    if "AI_RTC_FACADE_EVENT_TOKEN_WILL_EXPIRE" not in bridge_c:
        failures.append("bridge does not handle TOKEN_WILL_EXPIRE")
    if "[RTC_FACADE] token renew requested ret=%d" not in bridge_c:
        failures.append("bridge missing token renew request log")
    if "App_Rtc_Facade_Bridge_Set_Token_Request_Callback" not in entity_c:
        failures.append("entity_iot_process.c does not register token request callback")
    if "audio_engine_init_with_read_callback(App_Rtc_Facade_Bridge_Audio_Read_Callback, NULL)" not in ap_main:
        failures.append("ap_main.c does not route audio uplink through RTC facade bridge")

    try:
        ntwk_index = index_or_fail(ap_main, "ntwk_trans_init();", "network transfer init")
        event_index = index_or_fail(ap_main, "app_event_init();", "app event init")
        bridge_index = index_or_fail(ap_main, "App_Rtc_Facade_Bridge_Init();", "bridge init")
        audio_index = index_or_fail(ap_main, "audio_engine_init_with_read_callback", "audio engine facade callback init")
        video_index = index_or_fail(ap_main, "video_engine_init();", "video engine init")
        if not (ntwk_index < event_index < bridge_index < audio_index < video_index):
            failures.append("ap_main init order must be network_transfer, app_event, bridge, audio, video")
    except AssertionError as exc:
        failures.append(str(exc))

    if failures:
        print("FAIL: RTC facade W7 boundary check")
        for failure in failures:
            print(f"- {failure}")
        return 1

    print("PASS: RTC facade W7 boundary check")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
