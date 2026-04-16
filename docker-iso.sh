#!/bin/sh
set -e

REPO_ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
DOCKER_BIN=${DOCKER_BIN:-docker}
DOCKER_IMAGE=${DOCKER_IMAGE:-arawn780/gcc-cross-i686-elf:fast}

if ! command -v "$DOCKER_BIN" >/dev/null 2>&1; then
    echo "ERROR: Docker CLI not found (expected '$DOCKER_BIN')." >&2
    exit 1
fi

echo "==> Building makar.iso in Docker image: $DOCKER_IMAGE"
"$DOCKER_BIN" run --rm \
    -u "$(id -u):$(id -g)" \
    -v "$REPO_ROOT:/work" \
    -w /work \
    "$DOCKER_IMAGE" \
    bash -lc "bash iso.sh"

echo "==> Docker build complete: $REPO_ROOT/makar.iso"
