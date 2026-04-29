#!/bin/sh
# docker-hdd-boot.sh – boot Makar interactively from the installed HDD image.
#
# Mirrors docker-qemu.sh but uses -boot c (HDD) instead of -boot d (CD-ROM).
# If makar-hdd.img does not exist, generate-hdd.sh is called first.
#
# Usage: ./docker-hdd-boot.sh

set -e

REPO_ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
DOCKER_BIN=${DOCKER_BIN:-docker}
DOCKER_PLATFORM=${DOCKER_PLATFORM:-linux/amd64}
HDD_IMG=${HDD_IMG:-makar-hdd.img}

. ./src/config.sh
QEMU_BIN="qemu-system-$(./src/target-triplet-to-arch.sh "$HOST")"

if ! command -v "$QEMU_BIN" >/dev/null 2>&1; then
    echo "ERROR: host QEMU not found (expected '$QEMU_BIN')." >&2
    exit 1
fi

if [ ! -f "$REPO_ROOT/$HDD_IMG" ]; then
    echo "==> $HDD_IMG not found; running generate-hdd.sh..."
    HDD_IMG="$HDD_IMG" DOCKER_BIN="$DOCKER_BIN" DOCKER_PLATFORM="$DOCKER_PLATFORM" \
        "$REPO_ROOT/generate-hdd.sh"
else
    echo "==> Found $HDD_IMG."
fi

echo "==> Booting from HDD (Ctrl-A X to quit in -nographic, or close the window)..."

"$QEMU_BIN" \
    -drive file="$REPO_ROOT/$HDD_IMG",format=raw,if=ide,index=0 \
    -boot c \
    -serial stdio
