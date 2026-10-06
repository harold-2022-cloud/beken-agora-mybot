#!/usr/bin/env python3
"""Check production Device API endpoints are defined and selectable."""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "ap" / "device_api_client.c"
text = SRC.read_text(encoding="utf-8")

required = {
    "DEVICE_API_BASE_URL_PROD_CN": "http://mybot.sh2.agoralab.co/api",
    "DEVICE_API_BASE_URL_PROD_GLOBAL": "http://mybot.sg3.agoralab.co/api",
}

failed = []
for macro, url in required.items():
    pattern = rf"#define\s+{macro}\s+\"{re.escape(url)}\""
    if not re.search(pattern, text):
        failed.append(f"missing {macro} = {url}")

if not re.search(r"#define\s+DEVICE_API_DEFAULT_BASE_URL\s+DEVICE_API_BASE_URL_PROD_GLOBAL", text):
    failed.append("default base URL must reference DEVICE_API_BASE_URL_PROD_GLOBAL")

if failed:
    print("Device API endpoint contract failed:")
    for item in failed:
        print(f"- {item}")
    sys.exit(1)

print("Device API endpoint contract passed")
