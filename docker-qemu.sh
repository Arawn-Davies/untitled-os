#!/bin/sh
# docker-qemu.sh – build a normal (interactive) ISO and run it locally.
#
# Cleans first so that any stale TEST_MODE build artifacts from docker-ktest.sh
# do not contaminate this build.  The resulting kernel boots to the shell.
#
# Usage: ./docker-qemu.sh

set -e

REPO_ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
DOCKER_IMAGE=${DOCKER_IMAGE:-arawn780/gcc-cross-i686-elf:fast}
DOCKER_BIN=${DOCKER_BIN:-docker}

. ./src/config.sh
QEMU_BIN="qemu-system-$(./src/target-triplet-to-arch.sh "$HOST")"

echo "==> Cleaning build artifacts..."
"$DOCKER_BIN" run --rm \
    -v "$REPO_ROOT:/work" \
    -w /work \
    "$DOCKER_IMAGE" \
    bash -c '. ./src/config.sh && for p in $PROJECTS; do (cd $p && $MAKE clean 2>/dev/null || true); done'

echo "==> Building interactive ISO..."
CFLAGS='-O0 -g3' ./docker-iso.sh

if [ ! -f hdd.img ]; then
    qemu-img create -f raw hdd.img 512M
fi

if ! command -v "$QEMU_BIN" >/dev/null 2>&1; then
    echo "ERROR: host QEMU not found (expected '$QEMU_BIN')." >&2
    exit 1
fi

"$QEMU_BIN" \
    -drive file=hdd.img,format=raw,if=ide,index=0 \
    -drive file=makar.iso,if=ide,index=2,media=cdrom \
    -boot order=d \
    -serial stdio
