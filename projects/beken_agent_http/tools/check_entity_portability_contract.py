#!/usr/bin/env python3
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[4]
PROJECT = ROOT / "bk_solution_ai" / "projects" / "beken_genie_rino"
SDK = ROOT / "ai_iot_sdk"
CORE = SDK / "entity_iot_sdk"
CONTRACT = Path("/mnt/c/Users/harold.chen/esp32s3_ai_alarm/docs/porting/entity_porting_contract.md")
REPORT_GUARD = PROJECT / "tools" / "check_report_transport_contract.py"
TEMPLATE_DIR = SDK / "docs" / "templates" / "entity_interface_reference"
TEMPLATE_H = TEMPLATE_DIR / "entity_interface_reference.h"
TEMPLATE_C = TEMPLATE_DIR / "entity_interface_reference.c"
TEMPLATE_HOST_CHECK = SDK / "test" / "host" / "check_entity_interface_reference_template.py"
DESIGN_DOC = PROJECT / "docs" / "superpowers" / "specs" / "2026-07-01-entity-interface-reference-layer-design.md"


def fail(msg: str) -> None:
    print(f"FAIL: {msg}")
    sys.exit(1)


def read(path: Path) -> str:
    if not path.exists():
        fail(f"missing file: {path}")
    return path.read_text(encoding="utf-8")


def require_tokens(label: str, text: str, tokens: list[str]) -> None:
    for token in tokens:
        if token not in text:
            fail(f"{label} missing token: {token}")


def check_contract() -> None:
    text = read(CONTRACT)
    require_tokens(
        "entity_porting_contract.md",
        text,
        [
            "## Product Callback Safety Contract",
            "small-stack product callback",
            "must not perform blocking network IO",
            "must enqueue heavy work",
            "## Entity Interface Reference Layer Contract",
            "reference template, not product policy",
            "must not include vendor headers",
            "Temporary ESP32-S3 platform_adapter exception",
            "ai_dialog_diag.h bridge",
            "New ports must not copy this exception",
            "chip_bk7258/os/bsp_system_bk7258.c",
            "chip_bk7258/os/bsp_sync_bk7258.c",
            "chip_bk7258/storage/storage_ops_bk7258.c",
            "chip_bk7258/ota/ota_ops_bk7258.c",
            "check_entity_portability_contract.py",
        ],
    )


def check_report_guard() -> None:
    guard = read(REPORT_GUARD)
    require_tokens(
        "check_report_transport_contract.py",
        guard,
        [
            "Entity_Event_Report_Send",
            "Mqtt_Info.Mqtt_Host",
            "Entity_Http_Event_Report_Post_Json",
        ],
    )


def check_core_dependency_boundary() -> None:
    cmake = read(CORE / "CMakeLists.txt")
    contract = read(CONTRACT)
    active_cmake = "\n".join(
        line for line in cmake.splitlines()
        if not line.lstrip().startswith("#")
    )
    forbidden = [
        "REQUIRES chip_bk7258",
        "REQUIRES chip_esp32s3",
    ]
    for token in forbidden:
        if token in active_cmake:
            fail(f"entity_iot_sdk/CMakeLists.txt contains forbidden dependency: {token}")

    if "platform_adapter" in active_cmake:
        if "Temporary ESP32-S3 platform_adapter exception" not in contract:
            fail("entity_iot_sdk/CMakeLists.txt uses platform_adapter without a documented temporary exception")

        if "ai_dialog_diag.h" not in cmake:
            fail("platform_adapter exception must be documented as the ai_dialog_diag.h bridge")

        armino_branch = cmake.split("else()", 1)[0]
        armino_active = "\n".join(
            line for line in armino_branch.splitlines()
            if not line.lstrip().startswith("#")
        )
        if "platform_adapter" in armino_active:
            fail("armino/BK entity_iot_sdk branch must not depend on platform_adapter")

    for path in CORE.rglob("*.[ch]"):
        rel = path.relative_to(CORE)
        text = path.read_text(encoding="utf-8", errors="ignore")
        for token in [
            '#include "bk_',
            "#include <bk_",
            '#include "driver/',
            "#include <driver/",
            '#include "components/',
            "#include <components/",
        ]:
            if token in text:
                fail(f"portable core includes vendor/product header: {rel}: {token}")


def check_template() -> None:
    header = read(TEMPLATE_H)
    source = read(TEMPLATE_C)
    design = read(DESIGN_DOC)
    read(TEMPLATE_HOST_CHECK)

    require_tokens(
        "entity_interface_reference.h",
        header,
        [
            "Entity_Interface_Reference_Init",
            "Entity_Interface_Reference_Work_Enqueue",
            "Entity_Interface_Reference_Hooks",
        ],
    )
    require_tokens(
        "entity_interface_reference.c",
        source,
        [
            "ENTITY_INTERFACE_REF_WORK_QUEUE_DEPTH",
            "Entity_Interface_Reference_Worker",
            "must not call heavy Entity APIs from product callbacks",
            "Entity_Interface_Reference_Work_Enqueue",
        ],
    )
    require_tokens(
        "entity-interface-reference-layer-design",
        design,
        [
            "Product-owned behavior stays outside SDK core",
            "Callback safety",
            "Reference template",
            "No vendor headers",
        ],
    )

    for path, text in [(TEMPLATE_H, header), (TEMPLATE_C, source)]:
        for token in ["bk_", "esp_", "driver/", "components/"]:
            if token in text:
                fail(f"template contains vendor token {token}: {path}")

    result = subprocess.run(
        [sys.executable, str(TEMPLATE_HOST_CHECK)],
        cwd=TEMPLATE_HOST_CHECK.parent,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
    if result.returncode != 0:
        print(result.stdout, end="")
        fail("entity interface reference template host/static check failed")


def main() -> int:
    check_contract()
    check_report_guard()
    check_core_dependency_boundary()
    check_template()
    print("PASS: entity portability contract")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
