#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
PROJECT_DIR="${REPO_ROOT}/projects/beken_agent_http"
SDK_DIR="${SDK_DIR:-${REPO_ROOT}/bk_avdk_smp}"
TOOLCHAIN_DIR="${COMPILER_TOOLCHAIN_PATH:-/opt/gcc-arm-none-eabi-10.3-2021.10/bin}"
TARGET="bk7258"
CLEAN=0
COPY_TO=""
MAKE_ARGS=()

usage() {
    cat <<USAGE
Usage: $(basename "$0") [--clean] [--copy-to DIR] [make-args...]

Build beken_agent_http for BK7258.

Environment:
  SDK_DIR                  optional Beken SDK checkout; default: repo bk_avdk_smp submodule
  COMPILER_TOOLCHAIN_PATH  default: /opt/gcc-arm-none-eabi-10.3-2021.10/bin

Examples:
  tools/apply_beken_patches.sh
  tools/build_beken_agent_http.sh --clean
  tools/build_beken_agent_http.sh --clean --copy-to /mnt/c/Users/harold.chen/Desktop/bk-burn-tool/BEKEN_BKFIL_V3.0.1.4_314_20240924
USAGE
}

while (($# > 0)); do
    case "$1" in
        --clean)
            CLEAN=1
            shift
            ;;
        --copy-to)
            COPY_TO="${2:-}"
            if [[ -z "${COPY_TO}" ]]; then
                echo "ERROR: --copy-to requires a directory" >&2
                exit 1
            fi
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        --)
            shift
            MAKE_ARGS+=("$@")
            break
            ;;
        *)
            MAKE_ARGS+=("$1")
            shift
            ;;
    esac
done

if [[ ! -f "${SDK_DIR}/tools/build_tools/build_files/project_main.mk" ]]; then
    echo "ERROR: SDK_DIR does not look like a Beken SDK checkout: ${SDK_DIR}" >&2
    exit 1
fi

if [[ ! -x "${TOOLCHAIN_DIR}/arm-none-eabi-gcc" ]]; then
    echo "ERROR: arm-none-eabi-gcc not found under COMPILER_TOOLCHAIN_PATH: ${TOOLCHAIN_DIR}" >&2
    exit 1
fi

export COMPILER_TOOLCHAIN_PATH="${TOOLCHAIN_DIR}"

echo "REPO_ROOT=${REPO_ROOT}"
echo "PROJECT_DIR=${PROJECT_DIR}"
echo "SDK_DIR=${SDK_DIR}"
echo "COMPILER_TOOLCHAIN_PATH=${COMPILER_TOOLCHAIN_PATH}"
echo "TARGET=${TARGET}"

if [[ "${CLEAN}" -eq 1 ]]; then
    rm -rf "${PROJECT_DIR}/build"
fi

make -C "${PROJECT_DIR}" "${TARGET}" SDK_DIR="${SDK_DIR}" "${MAKE_ARGS[@]}"

PACKAGE_DIR="${PROJECT_DIR}/build/${TARGET}/beken_agent_http/package"
echo "firmware: ${PACKAGE_DIR}/all-app.bin"
echo "ota binary: ${PACKAGE_DIR}/app_pack.rbl"

if [[ -n "${COPY_TO}" ]]; then
    mkdir -p "${COPY_TO}"
    cp "${PACKAGE_DIR}/all-app.bin" "${COPY_TO}/beken_agent_http_mybot_all-app.bin"
    cp "${PACKAGE_DIR}/app_pack.rbl" "${COPY_TO}/beken_agent_http_mybot_app_pack.rbl"
    echo "copied: ${COPY_TO}/beken_agent_http_mybot_all-app.bin"
    echo "copied: ${COPY_TO}/beken_agent_http_mybot_app_pack.rbl"
fi

