#!/usr/bin/env python3
"""Check AP provisioning SSID is derived from chip UID short code."""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[3]
SRC = ROOT / "projects/beken_agent_http/ap/http_wifi_provision_server.c"
text = SRC.read_text(encoding="utf-8")

checks = [
    ("includes UID API", r"#include\s+<components/bk_uid\.h>"),
    ("reads chip UID", r"bk_uid_get_data\s*\("),
    ("uses R1 six digit UID code", r'"R1-%06X"'),
    ("keeps Bluetooth MAC fallback", r"bk_get_mac\s*\([^;]*MAC_TYPE_BLUETOOTH"),
    ("does not use old AG-BK prefix", r'"AG-BK-%02X%02X%02X"'),
]

failed = []
for name, pattern in checks[:4]:
    if not re.search(pattern, text, re.S):
        failed.append(f"missing: {name}")

if re.search(checks[4][1], text):
    failed.append("old AG-BK prefix is still present")

if failed:
    print("AP SSID contract failed:")
    for item in failed:
        print(f"- {item}")
    sys.exit(1)

print("AP SSID contract passed")
