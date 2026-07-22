#!/usr/bin/env python3
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[4]
SDK = ROOT / "ai_iot_sdk"
BK_SYSTEM = SDK / "chip_bk7258" / "os" / "bsp_system_bk7258.c"
CONTRACT = Path("/mnt/c/Users/harold.chen/esp32s3_ai_alarm/docs/porting/entity_porting_contract.md")
SPEC = Path("/mnt/c/Users/harold.chen/esp32s3_ai_alarm/docs/porting/allocator-guard-spec.md")


def fail(msg: str) -> None:
    print(f"FAIL: {msg}")
    sys.exit(1)


def read(path: Path) -> str:
    if not path.exists():
        fail(f"missing file: {path}")
    return path.read_text(encoding="utf-8", errors="ignore")


def require_tokens(label: str, text: str, tokens: list[str]) -> None:
    for token in tokens:
        if token not in text:
            fail(f"{label} missing token: {token}")


def func_body(src: str, name: str) -> str:
    match = re.search(rf"(?:static\s+)?[\w\s\*]+?\b{name}\s*\([^)]*\)\s*\{{", src)
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


def check_contract() -> None:
    text = read(CONTRACT)
    require_tokens(
        "entity_porting_contract.md",
        text,
        [
            "## Allocator Ownership Contract",
            "Each allocator owns the pointers it hands out",
            "same ownership tag/check",
            "magic tag",
            "foreign heap pointer",
            "OPT-5",
        ],
    )
    opt5_rows = [line for line in text.splitlines() if line.startswith("| OPT-5 |")]
    if not opt5_rows or "**SDK-optimizable** (fixed)" not in opt5_rows[0]:
        fail("OPT-5 backlog row is not marked fixed")


def check_spec() -> None:
    text = read(SPEC)
    require_tokens(
        "allocator-guard-spec.md",
        text,
        [
            "Header layout",
            "BSP_MEM_MAGIC_ALIVE",
            "BSP_MEM_MAGIC_FREED",
            "BSP_MEM_GUARD_DISABLE",
            "size > UINT32_MAX",
            "best-effort ownership check",
            "BK7258 `Bsp_Psram_*` is currently a documented alias",
        ],
    )
    if text.rstrip().endswith("```"):
        fail("allocator-guard-spec.md ends with a stray code fence")


def check_bk_allocator() -> None:
    src = read(BK_SYSTEM)
    require_tokens(
        "bsp_system_bk7258.c",
        src,
        [
            "BSP_MEM_MAGIC_ALIVE",
            "BSP_MEM_MAGIC_FREED",
            "Bsp_Mem_Guard_Check",
            "allocator misuse",
            "BSP_MEM_GUARD_DISABLE",
            "UINT32_MAX",
        ],
    )

    malloc_body = func_body(src, "Bsp_Mem_Malloc")
    free_body = func_body(src, "Bsp_Mem_Free")
    realloc_body = func_body(src, "Bsp_Mem_Realloc")

    if "BSP_MEM_MAGIC_ALIVE" not in malloc_body:
        fail("Bsp_Mem_Malloc does not tag live blocks")
    if "Bsp_Mem_Guard_Check" not in free_body:
        fail("Bsp_Mem_Free does not validate ownership")
    if "BSP_MEM_MAGIC_FREED" not in free_body:
        fail("Bsp_Mem_Free does not mark freed blocks")
    if "Bsp_Mem_Guard_Check" not in realloc_body:
        fail("Bsp_Mem_Realloc does not validate ownership")
    if "return NULL" not in realloc_body:
        fail("Bsp_Mem_Realloc mismatch path must return NULL")

    for name in [
        "Bsp_Psram_Malloc",
        "Bsp_Psram_Zalloc",
        "Bsp_Psram_Calloc",
        "Bsp_Psram_Realloc",
        "Bsp_Psram_Free",
    ]:
        body = func_body(src, name)
        if "Bsp_Mem_" not in body:
            fail(f"{name} is no longer documented as a Bsp_Mem_* alias")


def main() -> int:
    check_contract()
    check_spec()
    check_bk_allocator()
    print("PASS: allocator ownership contract")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
