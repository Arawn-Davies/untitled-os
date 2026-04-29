#!/bin/bash
# docker-hdd-test.sh – boot the installed HDD image under GDB and verify /hd mounts.
#
# What it does:
#   Step 1: Ensure makar-hdd.img exists.  If not, call generate-hdd.sh to build it.
#           generate-hdd.sh also builds the kernel binary if needed.
#   Step 2: Ensure src/kernel/makar.kernel (ELF with debug symbols) exists.
#           If not, perform a debug kernel build inside the cross-compiler container.
#   Step 3: Inside the cross-compiler container, boot makar-hdd.img with QEMU
#           using -boot c (HDD-only, no CD-ROM attached), launch a GDB session
#           against the kernel ELF, and run tests/gdb_hdd_test.py.
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
HDD_IMG=${HDD_IMG:-makar-hdd.img}

if ! command -v "$DOCKER_BIN" >/dev/null 2>&1; then
    echo "ERROR: Docker CLI not found (expected '$DOCKER_BIN')." >&2
    exit 1
fi

# ── Step 1: Ensure the HDD image exists ───────────────────────────────────────
if [ ! -f "$REPO_ROOT/$HDD_IMG" ]; then
    echo "==> $HDD_IMG not found; running generate-hdd.sh..."
    HDD_IMG="$HDD_IMG" DOCKER_BIN="$DOCKER_BIN" DOCKER_PLATFORM="$DOCKER_PLATFORM" \
        "$REPO_ROOT/generate-hdd.sh"
else
    echo "==> Found $HDD_IMG; skipping generation."
    echo "    (Delete $HDD_IMG to regenerate from the current kernel build.)"
fi

# ── Step 2: Ensure the kernel ELF (with debug symbols) exists ─────────────────
KERNEL_ELF="$REPO_ROOT/src/kernel/makar.kernel"
if [ ! -f "$KERNEL_ELF" ]; then
    echo "==> Kernel ELF not found; building debug kernel..."
    "$DOCKER_BIN" run --rm \
        --platform "$DOCKER_PLATFORM" \
        -u "$(id -u):$(id -g)" \
        -v "$REPO_ROOT:/work" \
        -w /work \
        "$DOCKER_IMAGE" \
        bash -lc "CFLAGS='-O0 -g3' bash build.sh"
fi

if [ ! -f "$KERNEL_ELF" ]; then
    echo "ERROR: $KERNEL_ELF still not found after build." >&2
    exit 1
fi

# ── Step 3: Boot HDD image + run GDB checks ───────────────────────────────────
echo "==> Booting $HDD_IMG under GDB (no CD-ROM attached)..."

"$DOCKER_BIN" run --rm \
    --platform "$DOCKER_PLATFORM" \
    -v "$REPO_ROOT:/work" \
    -w /work \
    "$DOCKER_IMAGE" \
    bash -c '
        # Launch QEMU with:
        #   - HDD as the only drive (index 0, boot c)
        #   - No CD-ROM — tests the HDD path without ISO interference
        #   - GDB stub (-s -S) so gdb_hdd_test.py can connect
        #   - Serial output captured to hdd-test-serial.log for post-mortem
        qemu-system-i386 \
            -drive file=/work/'"$HDD_IMG"',format=raw,if=ide,index=0 \
            -boot c \
            -serial file:/work/hdd-test-serial.log \
            -display none \
            -no-reboot \
            -no-shutdown \
            -s -S &
        QEMU_PID=$!

        # Give QEMU a moment to open its GDB port before we connect.
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
