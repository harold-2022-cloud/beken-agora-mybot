#!/usr/bin/env python3
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[4]
SDK = ROOT / "ai_iot_sdk" / "entity_iot_sdk"
REPORT_C = SDK / "entity_mqtt" / "entity_mqtt_event_report.c"
HTTP_C = SDK / "entity_http" / "entity_http_event_report.c"
REPORT_H = SDK / "entity_mqtt" / "entity_report.h"


def fail(msg: str) -> None:
    print(f"FAIL: {msg}")
    sys.exit(1)


def func_body(src: str, name: str) -> str:
    match = re.search(rf"void\s+{name}\s*\([^)]*\)\s*\{{", src)
    if not match:
        fail(f"{name} not found")

    depth = 0
    for idx in range(match.end() - 1, len(src)):
        if src[idx] == "{":
            depth += 1
        elif src[idx] == "}":
            depth -= 1
            if depth == 0:
                return src[match.end():idx]

    fail(f"{name} body not closed")


def main() -> int:
    if not REPORT_H.exists():
        fail("entity_report.h missing")

    report_src = REPORT_C.read_text(encoding="utf-8")
    http_src = HTTP_C.read_text(encoding="utf-8")
    report_header = REPORT_H.read_text(encoding="utf-8")

    for token in [
        "Entity_Report_Transport_Set",
        "Entity_Report_Transport_Get",
        "Entity_Report_Http_Endpoint_Set",
        "Entity_Event_Report_Send",
        "Entity_Report_Worker_Init",
    ]:
        if token not in report_header:
            fail(f"{token} missing from entity_report.h")

    for name in [
        "Entity_Mqtt_Event_Property_Report",
        "Entity_Mqtt_Event_Ota_Downloading_Report",
        "Entity_Mqtt_Event_Ota_Burning_Report",
    ]:
        body = func_body(report_src, name)
        if "Entity_Event_Report_Send" not in body:
            fail(f"{name} does not use Entity_Event_Report_Send")
        if "Entity_Http_Event_Report_Post" in body:
            fail(f"{name} still calls Entity_Http_Event_Report_Post")

    if "Mqtt_Info.Mqtt_Host" in http_src:
        fail("HTTP event report still derives URL from MQTT host")

    if "Entity_Http_Event_Report_Post_Json" not in http_src:
        fail("explicit JSON HTTP report API missing")

    print("PASS: report transport contract")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
