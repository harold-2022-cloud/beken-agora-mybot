#!/usr/bin/env python3
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[4]
SDK = ROOT / "ai_iot_sdk" / "entity_iot_sdk"
SYSTEM_C = SDK / "entity_app" / "entity_os_system.c"
CONFIG_NET_C = SDK / "entity_app" / "entity_config_net.c"
BLE_H = SDK / "entity_ble" / "entity_ble_gatt.h"
BLE_C = SDK / "entity_ble" / "entity_ble_gatt.c"


def fail(msg: str) -> None:
    print(f"FAIL: {msg}")
    sys.exit(1)


def read(path: Path) -> str:
    if not path.exists():
        fail(f"missing file: {path}")
    return path.read_text(encoding="utf-8", errors="ignore")


def func_body(src: str, name: str) -> str:
    match = re.search(rf"(?:static\s+)?(?:void|int|unsigned char)\s+{name}\s*\([^)]*\)\s*\{{", src)
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
    system_src = read(SYSTEM_C)
    config_src = read(CONFIG_NET_C)
    ble_h = read(BLE_H)
    ble_src = read(BLE_C)

    for token in [
        "Entity_Ble_Ensure_Init",
        "Entity_Ble_Is_Initialized",
    ]:
        if token not in ble_h:
            fail(f"{token} missing from entity_ble_gatt.h")
        if token not in ble_src:
            fail(f"{token} missing from entity_ble_gatt.c")

    init_body = func_body(system_src, "Entity_Iot_System_Init")
    if "Entity_System_Dev_State_Needs_Ble" not in system_src:
        fail("Entity_System_Dev_State_Needs_Ble missing")
    if "skip_Entity_Ble_Init" not in init_body:
        fail("Entity_Iot_System_Init does not log skip_Entity_Ble_Init")
    if "Entity_Ble_Ensure_Init" not in init_body:
        fail("Entity_Iot_System_Init does not conditionally ensure BLE init")

    ble_config_body = re.search(
        r"case\s+ENTITY_NET_MODE_BLE_CONFIG\s*:\s*\{(?P<body>.*?)break\s*;",
        config_src,
        re.S,
    )
    if not ble_config_body:
        fail("ENTITY_NET_MODE_BLE_CONFIG body not found")
    if "Entity_Ble_Ensure_Init" not in ble_config_body.group("body"):
        fail("BLE_CONFIG does not ensure BLE init before BLE operations")

    ble_bind_body = re.search(
        r"case\s+ENTITY_NET_MODE_BLE_BIND\s*:\s*\{(?P<body>.*?)break\s*;",
        config_src,
        re.S,
    )
    if not ble_bind_body:
        fail("ENTITY_NET_MODE_BLE_BIND body not found")
    if "Entity_Ble_Ensure_Init" not in ble_bind_body.group("body"):
        fail("BLE_BIND does not ensure BLE init before BLE operations")

    print("PASS: ble conditional init contract")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
