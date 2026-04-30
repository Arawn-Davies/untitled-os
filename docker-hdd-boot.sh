#!/bin/sh
# docker-hdd-boot.sh – build a full interactive kernel, install it to an HDD
# image, and boot it in QEMU.
#
# Mirrors docker-qemu.sh but boots -boot c (HDD) instead of CD-ROM.
# Always cleans and rebuilds so the image contains the current source,
# compiled without TEST_MODE, with the full shell + apps.
#
# Usage: ./docker-hdd-boot.sh

set -e

REPO_ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
DOCKER_IMAGE=${DOCKER_IMAGE:-arawn780/gcc-cross-i686-elf:fast}
DOCKER_BIN=${DOCKER_BIN:-docker}
DOCKER_PLATFORM=${DOCKER_PLATFORM:-linux/amd64}
HDD_IMG=${HDD_IMG:-makar-hdd.img}
export DOCKER_PLATFORM

. ./src/config.sh
QEMU_BIN="qemu-system-$(./src/target-triplet-to-arch.sh "$HOST")"

if ! command -v "$QEMU_BIN" >/dev/null 2>&1; then
    echo "ERROR: host QEMU not found (expected '$QEMU_BIN')." >&2
    exit 1
fi

echo "==> Cleaning build artifacts..."
"$DOCKER_BIN" run --rm \
    --platform "$DOCKER_PLATFORM" \
    -v "$REPO_ROOT:/work" \
    -w /work \
    "$DOCKER_IMAGE" \
    bash -c '. ./src/config.sh && for p in $PROJECTS; do (cd $p && $MAKE clean 2>/dev/null || true); done'

echo "==> Building interactive kernel (no TEST_MODE)..."
"$DOCKER_BIN" run --rm \
    --platform "$DOCKER_PLATFORM" \
    -u "$(id -u):$(id -g)" \
    -v "$REPO_ROOT:/work" \
    -w /work \
    "$DOCKER_IMAGE" \
    bash -lc "CFLAGS='-O0 -g3' bash build.sh"

echo "==> Generating HDD image..."
rm -f "$REPO_ROOT/$HDD_IMG"
HDD_IMG="$HDD_IMG" DOCKER_BIN="$DOCKER_BIN" DOCKER_PLATFORM="$DOCKER_PLATFORM" \
    "$REPO_ROOT/generate-hdd.sh"

echo "==> Booting from HDD..."
"$QEMU_BIN" \
    -drive file="$REPO_ROOT/$HDD_IMG",format=raw,if=ide,index=0 \
    -boot c \
    -serial stdio
