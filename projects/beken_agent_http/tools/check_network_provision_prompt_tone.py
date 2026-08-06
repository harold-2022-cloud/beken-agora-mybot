#!/usr/bin/env python3
"""Verify the Beken provisioning prompt tone is sourced from ESP32 wificonfig.ogg."""

from __future__ import annotations

import hashlib
import re
import shutil
import subprocess
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
ESP32_WIFI_CONFIG_OGG = Path("/mnt/c/Users/harold.chen/esp32-2/main/assets/locales/en-US/wificonfig.ogg")
BEKEN_PROMPT_HEADER = ROOT.parents[1] / "components" / "bk_app_event" / "prompt_tone_mp3_array.h"
ARRAY_NAME = "network_provision_prompt_tone_array"


def fail(message: str) -> None:
    raise SystemExit(f"FAIL: {message}")


def extract_c_array_bytes(header_text: str, array_name: str) -> bytes:
    match = re.search(
        rf"const char\s+{re.escape(array_name)}\s*\[\]\s*=\s*\{{(?P<body>.*?)\n\}};",
        header_text,
        re.DOTALL,
    )
    if not match:
        fail(f"missing {array_name} in {BEKEN_PROMPT_HEADER}")

    values = re.findall(r"0x([0-9A-Fa-f]{2})", match.group("body"))
    if not values:
        fail(f"{array_name} has no byte literals")

    return bytes(int(value, 16) for value in values)


def convert_expected_mp3() -> bytes:
    if not ESP32_WIFI_CONFIG_OGG.exists():
        fail(f"missing source prompt {ESP32_WIFI_CONFIG_OGG}")

    ffmpeg = shutil.which("ffmpeg")
    if ffmpeg is None:
        fail("ffmpeg is required to verify converted provisioning prompt")

    with tempfile.TemporaryDirectory() as tmp_dir:
        out_path = Path(tmp_dir) / "wificonfig_16k_mono_24k.mp3"
        subprocess.run(
            [
                ffmpeg,
                "-y",
                "-hide_banner",
                "-loglevel",
                "error",
                "-i",
                str(ESP32_WIFI_CONFIG_OGG),
                "-ar",
                "16000",
                "-ac",
                "1",
                "-b:a",
                "24k",
                str(out_path),
            ],
            check=True,
        )
        return out_path.read_bytes()


def main() -> None:
    actual = extract_c_array_bytes(BEKEN_PROMPT_HEADER.read_text(encoding="utf-8"), ARRAY_NAME)
    expected = convert_expected_mp3()

    if actual != expected:
        fail(
            f"{ARRAY_NAME} does not match converted ESP32 en-US wificonfig.ogg; "
            f"actual bytes={len(actual)} sha256={hashlib.sha256(actual).hexdigest()} "
            f"expected bytes={len(expected)} sha256={hashlib.sha256(expected).hexdigest()}"
        )

    print(
        f"PASS: {ARRAY_NAME} matches converted ESP32 en-US wificonfig.ogg "
        f"bytes={len(actual)} sha256={hashlib.sha256(actual).hexdigest()}"
    )


if __name__ == "__main__":
    main()
