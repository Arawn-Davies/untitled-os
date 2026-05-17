#!/bin/sh
# generate-hdd.sh - build an installable Makar HDD image bootable from MBR.
#
# Bootloader: Limine (v9.x, BIOS).  Replaces the previous GRUB 2 setup --
# Limine's `bios-install` writes a self-contained MBR stub that loads
# limine-bios.sys from the FAT32 partition, with no UUID-search dance and
# no rescue-shell fallout when the host environment can't probe loop
# devices.  GRUB 2 inside Docker had to be patched around with a hand-
# constructed core.img + manual MBR/embedding-area writes; Limine handles
# all of that in one `bios-install` call.
#
# What it produces:
#   makar-hdd.img   raw disk, MBR + FAT32 + Limine BIOS bootloader.
#
# Test with:
#   qemu-system-i386 -drive file=makar-hdd.img,format=raw,if=ide,index=0 \
#                    -serial stdio -boot c
#
# Env vars:
#   HDD_IMG      Output filename (default makar-hdd.img).
#   HDD_SIZE_MB  Image size (default 512).
#   KERNEL_ARGS  Optional kernel command-line baked into limine.conf
#                (e.g. KERNEL_ARGS=test_mode for automated test images).
#   LIMINE_VER   Limine release to fetch (default 9.6.5).  The binary
#                tarball is cached in vendor/limine/ to avoid re-downloading
#                across rebuilds; delete that directory to force a refresh.
#
# Requirements on the host: Docker (everything else runs in a container).

set -e

REPO_ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
DOCKER_BIN=${DOCKER_BIN:-docker}
DOCKER_PLATFORM=${DOCKER_PLATFORM:-linux/amd64}
BUILD_IMAGE=${BUILD_IMAGE:-arawn780/gcc-cross-i686-elf:fast}
HDD_IMG=${HDD_IMG:-makar-hdd.img}
HDD_SIZE_MB=${HDD_SIZE_MB:-512}
LIMINE_VER=${LIMINE_VER:-9.6.5}

for _arg in "$@"; do
    case "$_arg" in
        *) echo "ERROR: unknown flag '$_arg'" >&2; exit 1 ;;
    esac
done

# Build the kernel if not already present.
if [ ! -f "$REPO_ROOT/sysroot/boot/makar.kernel" ]; then
    echo "==> Building interactive kernel..."
    "$DOCKER_BIN" run --rm \
        --platform "$DOCKER_PLATFORM" \
        -u "$(id -u):$(id -g)" \
        -v "$REPO_ROOT:/work" \
        -w /work \
        "$BUILD_IMAGE" \
        bash -lc "CFLAGS='-O0 -g3' bash build.sh"
else
    echo "==> Kernel binary found; skipping rebuild."
    echo "    (Delete sysroot/boot/makar.kernel to force a fresh build.)"
fi

KERNEL="$REPO_ROOT/sysroot/boot/makar.kernel"
if [ ! -f "$KERNEL" ]; then
    echo "ERROR: $KERNEL not found after build." >&2
    exit 1
fi

# Fetch Limine release into vendor/limine.  Cached across rebuilds; delete
# the directory (or change LIMINE_VER) to force a refresh.
LIMINE_DIR="$REPO_ROOT/vendor/limine/v$LIMINE_VER"
if [ ! -f "$LIMINE_DIR/limine-bios.sys" ] || [ ! -x "$LIMINE_DIR/limine" ]; then
    echo "==> Fetching Limine v$LIMINE_VER..."
    mkdir -p "$LIMINE_DIR"
    "$DOCKER_BIN" run --rm \
        --platform "$DOCKER_PLATFORM" \
        -v "$REPO_ROOT:/work" \
        -w "/work/vendor/limine/v$LIMINE_VER" \
        "$BUILD_IMAGE" \
        bash -lc "set -e
            curl -fsSL https://github.com/limine-bootloader/limine/releases/download/v${LIMINE_VER}-binary/limine-${LIMINE_VER}-binary.tar.gz \
                | tar xz --strip-components=1
            chmod +x limine"
fi

# Now build the image inside the container, with the Limine binary mounted
# alongside the workspace.
echo "==> Creating $HDD_IMG (${HDD_SIZE_MB} MiB, Limine v$LIMINE_VER)..."

"$DOCKER_BIN" run --rm -i \
    --platform "$DOCKER_PLATFORM" \
    --privileged \
    -e HDD_IMG="$HDD_IMG" \
    -e HDD_SIZE_MB="$HDD_SIZE_MB" \
    -e HOST_UID="$(id -u)" \
    -e HOST_GID="$(id -g)" \
    -e KERNEL_ARGS="${KERNEL_ARGS:-}" \
    -e LIMINE_VER="$LIMINE_VER" \
    -v "$REPO_ROOT:/work" \
    -w /work \
    "$BUILD_IMAGE" \
    bash << 'INNER'
set -e

HDD="/work/$HDD_IMG"
MNT=/mnt/makar-install
LIMINE="/work/vendor/limine/v$LIMINE_VER"

rm -f "$HDD"
dd if=/dev/zero of="$HDD" bs=1M count="$HDD_SIZE_MB" status=none

# MBR + single bootable FAT32-LBA partition starting at LBA 2048.
sfdisk "$HDD" << 'PTAB'
label: dos
start=2048, type=c, bootable
PTAB

PART_OFFSET=$(( 2048 * 512 ))
LOOP=$(losetup --find --show "$HDD")
PART=$(losetup --find --show --offset="$PART_OFFSET" "$HDD")
cleanup() {
    umount "$MNT"  2>/dev/null || true
    rmdir  "$MNT"  2>/dev/null || true
    losetup -d "$PART" 2>/dev/null || true
    losetup -d "$LOOP" 2>/dev/null || true
}
trap cleanup EXIT

mkfs.fat -F 32 -n MAKAR "$PART"
mkdir -p "$MNT"
mount "$PART" "$MNT"

# Kernel + userspace apps.
mkdir -p "$MNT/boot"
cp /work/sysroot/boot/makar.kernel "$MNT/boot/makar.kernel"
if [ -d /work/isodir/apps ]; then
    mkdir -p "$MNT/apps"
    cp -r /work/isodir/apps/. "$MNT/apps/"
fi

# Limine BIOS stage 2 lives on the FAT32 partition; the MBR stub loads it.
cp "$LIMINE/limine-bios.sys" "$MNT/limine-bios.sys"

# Limine v9 config syntax (key=value, ":<entry>" for menu items).  We use
# the multiboot2 protocol so the kernel sees the same MB2 info structure
# it gets from GRUB on the ISO path -- no kernel-side changes needed.
cat > "$MNT/limine.conf" << LCFG
timeout: 3

/Makar OS
    protocol: multiboot2
    kernel_path: boot():/boot/makar.kernel
${KERNEL_ARGS:+    kernel_cmdline: $KERNEL_ARGS}
LCFG

sync
umount "$MNT"
rmdir  "$MNT"
losetup -d "$PART"
losetup -d "$LOOP"
trap - EXIT

# Write the Limine MBR stub.  `bios-install` figures out the partition
# layout from the GPT/MBR on the image and embeds itself in the boot
# sector + sectors immediately following, pointing at limine-bios.sys on
# the FAT32 partition.
"$LIMINE/limine" bios-install "$HDD"

chown "${HOST_UID}:${HOST_GID}" "$HDD" 2>/dev/null || true

echo "  Limine installed; image ready: $HDD"
INNER

echo ""
echo "==> Done.  To boot in QEMU:"
echo ""
printf '    qemu-system-i386 \\\n'
printf '        -drive file=%s,format=raw,if=ide,index=0 \\\n' "$HDD_IMG"
printf '        -serial stdio \\\n'
printf '        -boot c\n'
echo ""
echo "    To install onto a physical disk (DANGEROUS -- this wipes the"
echo "    target device):"
echo ""
printf '        sudo dd if=%s of=/dev/sdX bs=1M status=progress conv=fsync\n' "$HDD_IMG"
