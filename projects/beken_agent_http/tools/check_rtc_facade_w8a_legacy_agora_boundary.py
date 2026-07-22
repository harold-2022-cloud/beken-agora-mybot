#!/usr/bin/env python3
"""Static checks for Phase 7 W8a legacy Agora build boundaries."""

from __future__ import annotations

import re
from pathlib import Path


ROOT = Path("/root/smp/bk_solution_ai")
PROJECT = ROOT / "projects/beken_genie_rino"
NETWORK_TRANSFER = ROOT / "components/network_transfer"

LEGACY_AGORA_SOURCES = (
    "agora_rtc/bk_agora_api.c",
    "agora_rtc/agora_rtc_engine.c",
    "agora_rtc/agora_agent_engine.c",
    "agora_rtc/agent_utils/AgoraWebClientUtils.c",
    "agora_rtc/agora_rtc_msg_process.c",
)

LEGACY_AGORA_INCLUDE_LINES = (
    "agora_rtc",
    "agora_rtc/agent_utils",
)


def append_file_failures(failures: list[str], path: Path, pattern: str, message: str) -> None:
    text = path.read_text(encoding="utf-8")
    if re.search(pattern, text):
        failures.append(message)


def main() -> int:
    failures: list[str] = []

    cmake_path = NETWORK_TRANSFER / "CMakeLists.txt"
    network_transfer_c_path = NETWORK_TRANSFER / "network_transfer.c"
    cmake = cmake_path.read_text(encoding="utf-8")
    network_transfer_c = network_transfer_c_path.read_text(encoding="utf-8")

    for source in LEGACY_AGORA_SOURCES:
        if source in cmake:
            failures.append(f"network_transfer CMake still lists legacy Agora source: {source}")

    cmake_lines = [line.strip() for line in cmake.splitlines()]
    for include_dir in LEGACY_AGORA_INCLUDE_LINES:
        if include_dir in cmake_lines:
            failures.append(f"network_transfer CMake still exposes legacy Agora include path: {include_dir}")

    if '#include "bk_agora_api.h"' in network_transfer_c:
        failures.append("network_transfer.c still includes bk_agora_api.h")
    if re.search(r"\bbk_agora_\w+\s*\(", network_transfer_c):
        failures.append("network_transfer.c still calls a bk_agora_ API")

    for path in sorted((PROJECT / "ap").glob("*.[ch]")):
        append_file_failures(
            failures,
            path,
            r"\bbk_agora_\w+\s*\(",
            f"product AP directly calls a bk_agora_ API: {path}",
        )

    if failures:
        print("FAIL: RTC facade W8a legacy Agora boundary check")
        for failure in failures:
            print(f"- {failure}")
        return 1

    print("PASS: RTC facade W8a legacy Agora boundary check")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
