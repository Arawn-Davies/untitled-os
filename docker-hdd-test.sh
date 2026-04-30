#!/bin/bash
# docker-hdd-test.sh – build a fresh HDD image and run a GDB boot test.
#
# Produces a dedicated makar-hdd-test.img (separate from the interactive
# makar-hdd.img) so that test runs never share state with interactive boots.
#
# What it does:
#   Step 1: Clean and rebuild the interactive kernel so that
#           src/kernel/makar.kernel (GDB ELF) matches the binary in the image.
#   Step 2: Delete makar-hdd-test.img and regenerate it via generate-hdd.sh
#           (no --test flag: interactive kernel, shell_run called, VFS mounts).
#   Step 3: Boot the test image under GDB and run tests/gdb_hdd_test.py.
#
# Checks performed by gdb_hdd_test.py:
#   - Multiboot 2 magic (0x36D76289) in %eax at _start
#   - All standard boot checkpoints through shell_run
#   - CR0.PG set, CR3 non-zero, PIT ticking
#   - fat32_mounted() non-zero  →  /hd is mounted from the FAT32 partition
#
# Exit codes: 0 = all checks passed, 1 = any failure or timeout.
#
# Usage: ./docker-hdd-test.sh

set -e

REPO_ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
DOCKER_IMAGE=${DOCKER_IMAGE:-arawn780/gcc-cross-i686-elf:fast}
DOCKER_BIN=${DOCKER_BIN:-docker}
DOCKER_PLATFORM=${DOCKER_PLATFORM:-linux/amd64}
HDD_TEST_IMG=${HDD_TEST_IMG:-makar-hdd-test.img}

if ! command -v "$DOCKER_BIN" >/dev/null 2>&1; then
    echo "ERROR: Docker CLI not found (expected '$DOCKER_BIN')." >&2
    exit 1
fi

# ── Step 1: Clean and rebuild the kernel ──────────────────────────────────────
# Always rebuild so src/kernel/makar.kernel (the GDB symbol file) matches
# the binary installed into the HDD image.
echo "==> Cleaning build artifacts..."
"$DOCKER_BIN" run --rm \
    --platform "$DOCKER_PLATFORM" \
    -v "$REPO_ROOT:/work" \
    -w /work \
    "$DOCKER_IMAGE" \
    bash -c '. ./src/config.sh && for p in $PROJECTS; do (cd $p && $MAKE clean 2>/dev/null || true); done'

echo "==> Building kernel (interactive, -O0 -g3)..."
"$DOCKER_BIN" run --rm \
    --platform "$DOCKER_PLATFORM" \
    -u "$(id -u):$(id -g)" \
    -v "$REPO_ROOT:/work" \
    -w /work \
    "$DOCKER_IMAGE" \
    bash -lc "CFLAGS='-O0 -g3' bash build.sh"

KERNEL_ELF="$REPO_ROOT/src/kernel/makar.kernel"
if [ ! -f "$KERNEL_ELF" ]; then
    echo "ERROR: $KERNEL_ELF not found after build." >&2
    exit 1
fi

# ── Step 2: Generate fresh HDD test image ─────────────────────────────────────
# Use a dedicated image name so interactive and test images are independent.
# No --test flag: the GDB boot test requires the interactive kernel path
# (shell_run must be called so VFS auto-mounts the FAT32 partition).
echo "==> Generating $HDD_TEST_IMG..."
rm -f "$REPO_ROOT/$HDD_TEST_IMG"
HDD_IMG="$HDD_TEST_IMG" DOCKER_BIN="$DOCKER_BIN" DOCKER_PLATFORM="$DOCKER_PLATFORM" \
    "$REPO_ROOT/generate-hdd.sh"

# ── Step 3: Boot HDD image + run GDB checks ───────────────────────────────────
echo "==> Booting $HDD_TEST_IMG under GDB (no CD-ROM attached)..."

"$DOCKER_BIN" run --rm \
    --platform "$DOCKER_PLATFORM" \
    -v "$REPO_ROOT:/work" \
    -w /work \
    "$DOCKER_IMAGE" \
    bash -c '
        qemu-system-i386 \
            -drive file=/work/'"$HDD_TEST_IMG"',format=raw,if=ide,index=0 \
            -boot c \
            -serial file:/work/hdd-test-serial.log \
            -display none \
            -no-reboot \
            -no-shutdown \
            -s -S &
        QEMU_PID=$!

        sleep 2

        timeout 60 gdb-multiarch -batch \
            -ex "source tests/gdb_hdd_test.py" \
            src/kernel/makar.kernel \
            2>&1 | tee /work/hdd-test-gdb.log
        GDB_EXIT=${PIPESTATUS[0]}

        kill "$QEMU_PID" 2>/dev/null || true
        wait "$QEMU_PID" 2>/dev/null || true

        exit "$GDB_EXIT"
    '

echo ""
echo "==> HDD boot test PASSED."
echo "    GDB log:    hdd-test-gdb.log"
echo "    Serial log: hdd-test-serial.log"
