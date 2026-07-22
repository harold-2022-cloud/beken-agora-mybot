#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
SDK_DIR="${SDK_DIR:-${REPO_ROOT}/bk_avdk_smp}"
PATCH_DIR="${REPO_ROOT}/patches/beken"

if [[ ! -d "${SDK_DIR}/.git" ]]; then
    echo "ERROR: Beken SDK checkout not found: ${SDK_DIR}" >&2
    echo "Run: git submodule update --init --recursive" >&2
    exit 1
fi

for patch in "${PATCH_DIR}"/*.patch; do
    [[ -e "${patch}" ]] || continue
    echo "Applying Beken patch: ${patch}"
    if git -C "${SDK_DIR}" apply --check "${patch}" 2>/dev/null; then
        git -C "${SDK_DIR}" apply "${patch}"
    elif git -C "${SDK_DIR}" apply -R --check "${patch}" 2>/dev/null; then
        echo "Already applied: ${patch}"
    else
        echo "ERROR: patch cannot be applied cleanly: ${patch}" >&2
        exit 1
    fi
done

echo "Beken patches ready."
