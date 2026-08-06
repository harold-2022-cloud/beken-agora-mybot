#!/usr/bin/env python3
"""Check AP provisioning SSID is derived from Bluetooth MAC suffix."""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[3]
SRC = ROOT / "projects/beken_agent_http/ap/http_wifi_provision_server.c"
text = SRC.read_text(encoding="utf-8")

checks = [
    ("uses Bluetooth MAC", r"bk_get_mac\s*\([^;]*MAC_TYPE_BLUETOOTH"),
    ("uses R1 prefix", r'"R1-%02X%02X%02X"'),
    ("uses last three bytes in order", r"mac\[3\]\s*,\s*\n?\s*mac\[4\]\s*,\s*\n?\s*mac\[5\]"),
    ("does not use old AG-BK prefix", r'"AG-BK-%02X%02X%02X"'),
]

failed = []
for name, pattern in checks[:3]:
    if not re.search(pattern, text, re.S):
        failed.append(f"missing: {name}")

if re.search(checks[3][1], text):
    failed.append("old AG-BK prefix is still present")

if failed:
    print("AP SSID contract failed:")
    for item in failed:
        print(f"- {item}")
    sys.exit(1)

print("AP SSID contract passed")
