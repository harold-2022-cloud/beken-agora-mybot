#!/usr/bin/env python3
"""Regression tests for check_rtc_facade_runtime_log.py."""

from __future__ import annotations

import subprocess
import sys
import tempfile
from pathlib import Path


CHECKER = Path(__file__).with_name("check_rtc_facade_runtime_log.py")


def run_checker(log_text: str, *args: str) -> subprocess.CompletedProcess[str]:
    with tempfile.TemporaryDirectory() as tmpdir:
        log_path = Path(tmpdir) / "runtime.log"
        log_path.write_text(log_text, encoding="utf-8")
        return subprocess.run(
            [sys.executable, str(CHECKER), *args, str(log_path)],
            check=False,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )


def test_facade_runtime_sequence_passes_without_init_marker() -> None:
    log = """
ap1:I(25219):[RTC] >>> Ai_Rtc_Facade_On_Token_Result channel=stn_demo
ap1:I(25219):[RTC_FACADE] event=0 state=1 detail=0
ap1:I(25219):[RTC_FACADE] event=0 state=2 detail=0
ap1:I(25219):[RTC_FACADE] event=0 state=3 detail=0
ap0:I(27576):[RTC_FACADE] event=1 state=4 detail=2317
ap1:I(27795):[RTC_FACADE] event=4 state=4 detail=16133
ap0:app_evt:I(27801):APP_EVT_AGENT_JOINED
ap0:ntwk_tra:I(27594):[RTC_FACADE_TX] frame=1 size=160 format=0
ap0:I(28638):[RTC_FACADE_RX] frame=1 size=640 format=0
ap1:I(66372):[RTC_FACADE_DS] stream=49153 uid=16133 len=233 ts=755939850
ap0:I(70411):[RTC] GPIO_12 pressed: session_active=1 t=70411ms
ap0:I(70413):[PRODUCT_WORK] ai stop
ap0:I(70413):[RTC_FACADE] event=0 state=6 detail=0
ap1:I(70445):[RTC_FACADE] event=8 state=0 detail=0
ap1:app_evt:I(70445):APP_EVT_AGORA_SESSION_STOP
"""
    result = run_checker(log, "--require-datastream")
    assert result.returncode == 0, result.stdout + result.stderr


def test_empty_log_fails() -> None:
    result = run_checker("")
    assert result.returncode == 1
    assert "missing token forwarded to facade" in result.stdout


def test_token_renew_gate_passes() -> None:
    log = """
ap1:I(25219):[RTC] >>> Ai_Rtc_Facade_On_Token_Result channel=stn_demo
ap1:I(25219):[RTC_FACADE] event=0 state=1 detail=0
ap1:I(25219):[RTC_FACADE] event=0 state=2 detail=0
ap1:I(25219):[RTC_FACADE] event=0 state=3 detail=0
ap0:I(27576):[RTC_FACADE] event=1 state=4 detail=2317
ap1:I(27795):[RTC_FACADE] event=4 state=4 detail=16133
ap0:app_evt:I(27801):APP_EVT_AGENT_JOINED
ap0:ntwk_tra:I(27594):[RTC_FACADE_TX] frame=1 size=160 format=0
ap0:I(28638):[RTC_FACADE_RX] frame=1 size=640 format=0
ap1:I(50000):[RTC_FACADE] event=6 state=4 detail=0
ap1:I(50001):[DEVICE_ACCESS] request submitted
ap1:I(50002):[RTC_FACADE] token renew requested ret=0
ap1:I(51000):[RTC] >>> Ai_Rtc_Facade_On_Token_Result channel=stn_demo
ap0:I(70411):[RTC] GPIO_12 pressed: session_active=1 t=70411ms
ap0:I(70413):[PRODUCT_WORK] ai stop
ap1:I(70445):[RTC_FACADE] event=8 state=0 detail=0
ap1:app_evt:I(70445):APP_EVT_AGORA_SESSION_STOP
"""
    result = run_checker(log, "--require-token-renew")
    assert result.returncode == 0, result.stdout + result.stderr


def main() -> int:
    tests = [
        test_facade_runtime_sequence_passes_without_init_marker,
        test_empty_log_fails,
        test_token_renew_gate_passes,
    ]
    for test in tests:
        test()
    print("PASS: check_rtc_facade_runtime_log regression tests")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
