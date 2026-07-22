#!/usr/bin/env python3
"""Validate BK7258 RTC facade runtime UART logs for Phase 7 W6."""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path


REQUIRED_PATTERNS = (
    ("token forwarded to facade", re.compile(r"\[RTC\] >>> Ai_Rtc_Facade_On_Token_Result")),
    ("token ready state", re.compile(r"\[RTC_FACADE\] event=0 state=1 detail=0")),
    ("starting state", re.compile(r"\[RTC_FACADE\] event=0 state=2 detail=0")),
    ("joining state", re.compile(r"\[RTC_FACADE\] event=0 state=3 detail=0")),
    ("joined state", re.compile(r"\[RTC_FACADE\] event=1 state=4 detail=")),
    ("remote agent joined", re.compile(r"\[RTC_FACADE\] event=4 state=4 detail=")),
    ("agent joined app event", re.compile(r"APP_EVT_AGENT_JOINED")),
)

STOP_PATTERNS = (
    ("stop requested", re.compile(r"\[PRODUCT_WORK\] ai stop|\[RTC\] GPIO_12 pressed: session_active=1")),
    ("stopped state", re.compile(r"\[RTC_FACADE\] event=8 state=0 detail=")),
    ("stop app event", re.compile(r"APP_EVT_AGORA_SESSION_STOP")),
)

FATAL_PATTERNS = (
    ("agent start failure", re.compile(r"APP_EVT_AGENT_START_FAIL")),
    ("facade bridge init failure", re.compile(r"\[RTC_FACADE\] bridge init failed")),
    ("facade token failure", re.compile(r"\[RTC_FACADE\] token result failed")),
    ("facade failed event", re.compile(r"\[RTC_FACADE\] event=9 state=7")),
    ("assert", re.compile(r"\bASSERT\b|BK_ASSERT|assert failed", re.IGNORECASE)),
    ("panic", re.compile(r"\bpanic\b|hard fault|exception|backtrace", re.IGNORECASE)),
    ("watchdog", re.compile(r"\bwatchdog\b|\bwdt\b", re.IGNORECASE)),
    ("serial read failure", re.compile(r"Error reading from serial device")),
)

LEGACY_DIRECT_PATTERNS = (
    ("legacy bk_agora stop", re.compile(r"Agora RTC stopped successfully|Agora RTC destroyed successfully")),
    ("legacy credential setter", re.compile(r"bk_agora_set_mqtt_credentials")),
)


def find_max_counter(text: str, tag: str) -> int:
    pattern = re.compile(rf"\[{re.escape(tag)}\]\s+frame=(\d+)\s+size=(\d+)")
    max_frame = 0
    for match in pattern.finditer(text):
        max_frame = max(max_frame, int(match.group(1)))
    return max_frame


def check_pattern_group(text: str, group: tuple[tuple[str, re.Pattern[str]], ...]) -> list[str]:
    missing: list[str] = []
    for name, pattern in group:
        if not pattern.search(text):
            missing.append(name)
    return missing


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Validate a Phase 7 W6 BK7258 RTC facade runtime UART log."
    )
    parser.add_argument("log", type=Path, help="UART monitor log captured from the BK7258 board")
    parser.add_argument("--min-tx-frames", type=int, default=1, help="minimum logged RTC facade tx frame count")
    parser.add_argument("--min-rx-frames", type=int, default=1, help="minimum logged RTC facade rx frame count")
    parser.add_argument("--require-datastream", action="store_true", help="require at least one RTC facade datastream rx log")
    parser.add_argument("--require-reconnect", action="store_true", help="require reconnecting and rejoined facade events")
    parser.add_argument(
        "--require-token-renew",
        action="store_true",
        help="optional extended validation: require token will-expire and renew request logs",
    )
    parser.add_argument("--no-require-stop", action="store_true", help="do not require the manual AI stop path")
    args = parser.parse_args()

    try:
        text = args.log.read_text(encoding="utf-8", errors="replace")
    except OSError as exc:
        print(f"FAIL: cannot read {args.log}: {exc}", file=sys.stderr)
        return 2

    failures: list[str] = []

    failures.extend(f"missing {name}" for name in check_pattern_group(text, REQUIRED_PATTERNS))

    if not args.no_require_stop:
        failures.extend(f"missing {name}" for name in check_pattern_group(text, STOP_PATTERNS))

    tx_frames = find_max_counter(text, "RTC_FACADE_TX")
    rx_frames = find_max_counter(text, "RTC_FACADE_RX")
    if tx_frames < args.min_tx_frames:
        failures.append(f"RTC_FACADE_TX frames {tx_frames} < {args.min_tx_frames}")
    if rx_frames < args.min_rx_frames:
        failures.append(f"RTC_FACADE_RX frames {rx_frames} < {args.min_rx_frames}")

    if args.require_datastream and "[RTC_FACADE_DS]" not in text:
        failures.append("missing datastream rx")

    if args.require_reconnect:
        reconnect_patterns = (
            ("reconnecting event", re.compile(r"\[RTC_FACADE\] event=2 state=5 detail=")),
            ("rejoined event", re.compile(r"\[RTC_FACADE\] event=3 state=4 detail=")),
            ("rejoin app event", re.compile(r"APP_EVT_RTC_REJOIN_SUCCESS")),
        )
        failures.extend(f"missing {name}" for name in check_pattern_group(text, reconnect_patterns))

    if args.require_token_renew:
        token_renew_patterns = (
            ("token will expire event", re.compile(r"\[RTC_FACADE\] event=6 state=4 detail=0")),
            ("token renew request", re.compile(r"\[RTC_FACADE\] token renew requested ret=0")),
            ("renew token forwarded to facade", re.compile(r"\[RTC\] >>> Ai_Rtc_Facade_On_Token_Result channel=")),
        )
        failures.extend(f"missing {name}" for name in check_pattern_group(text, token_renew_patterns))

    for name, pattern in FATAL_PATTERNS + LEGACY_DIRECT_PATTERNS:
        if pattern.search(text):
            failures.append(f"unexpected {name}")

    if failures:
        print("FAIL: RTC facade runtime log validation failed")
        for failure in failures:
            print(f"- {failure}")
        print(f"observed RTC_FACADE_TX frames: {tx_frames}")
        print(f"observed RTC_FACADE_RX frames: {rx_frames}")
        return 1

    print("PASS: RTC facade runtime log")
    print(f"observed RTC_FACADE_TX frames: {tx_frames}")
    print(f"observed RTC_FACADE_RX frames: {rx_frames}")
    if args.require_datastream:
        print("observed RTC_FACADE_DS")
    if args.require_reconnect:
        print("observed reconnect and rejoin")
    if args.require_token_renew:
        print("observed token renew")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
