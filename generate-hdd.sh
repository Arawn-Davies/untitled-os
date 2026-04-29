#!/bin/sh
# generate-hdd.sh — build an installed Makar HDD image bootable with -boot c.
#
# What it produces:
#   makar-hdd.img   512 MiB raw disk, MBR + FAT32 + GRUB 2 in embedding area
#
# Test with:
#   qemu-system-i386 \
#       -drive file=makar-hdd.img,format=raw,if=ide,index=0 \
#       -serial stdio \
#       -boot c
#
# Why a separate script rather than the in-kernel installer?
#
#   The in-kernel installer copies core.img verbatim from the ISO. That
#   core.img was built with a generic `search` that scans every attached
#   device for /boot/grub/grub.cfg. When the ISO CD-ROM is also connected
#   during HDD boot, GRUB can find the CD's grub.cfg first and report the
#   CD-ROM's BIOS device number as the Multiboot2 biosdev tag. The kernel
#   maps biosdev - 0x80 directly to an IDE drive index; if that index is
#   wrong (or the HDD is the hint drive that already failed), vfs_auto_mount
#   may skip the installed FAT32 partition entirely.
#
#   This script uses grub-install instead, which builds core.img with the
#   HDD FAT32 partition as its explicit root — no device search, no
#   ambiguity. biosdev is always 0x80 (first HDD), and the kernel mounts
#   /hd reliably.
#
# Requirements on the host: Docker (all other tools run inside a container).

set -e

REPO_ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
DOCKER_BIN=${DOCKER_BIN:-docker}
DOCKER_PLATFORM=${DOCKER_PLATFORM:-linux/amd64}
BUILD_IMAGE=${BUILD_IMAGE:-arawn780/gcc-cross-i686-elf:fast}
HDD_IMG=${HDD_IMG:-makar-hdd.img}
HDD_SIZE_MB=${HDD_SIZE_MB:-512}

# ---------------------------------------------------------------------------
# Step 1: build the kernel (produces sysroot/boot/makar.kernel + makar.iso).
# Skip if the kernel is already up to date.
# ---------------------------------------------------------------------------
if [ ! -f "$REPO_ROOT/sysroot/boot/makar.kernel" ]; then
    echo "==> Building kernel..."
    "$DOCKER_BIN" run --rm \
        --platform "$DOCKER_PLATFORM" \
        -u "$(id -u):$(id -g)" \
        -v "$REPO_ROOT:/work" \
        -w /work \
        "$BUILD_IMAGE" \
        bash -lc "bash build.sh"
else
    echo "==> Kernel binary found; skipping rebuild."
    echo "    (Delete sysroot/boot/makar.kernel to force a fresh build.)"
fi

KERNEL="$REPO_ROOT/sysroot/boot/makar.kernel"
if [ ! -f "$KERNEL" ]; then
    echo "ERROR: $KERNEL not found after build." >&2
    exit 1
fi

# ---------------------------------------------------------------------------
# Step 2: create the HDD image inside an Ubuntu container that has
# grub-pc (grub-install, grub-mkimage) and dosfstools (mkfs.fat).
# These tools are absent from the cross-compiler image.
#
# All loop-device work runs as root inside the container; the final image
# is chown-ed back to the calling user before the container exits.
# ---------------------------------------------------------------------------
echo "==> Creating $HDD_IMG (${HDD_SIZE_MB} MiB)..."

"$DOCKER_BIN" run --rm -i \
    --platform "$DOCKER_PLATFORM" \
    --privileged \
    -e HDD_IMG="$HDD_IMG" \
    -e HDD_SIZE_MB="$HDD_SIZE_MB" \
    -e HOST_UID="$(id -u)" \
    -e HOST_GID="$(id -g)" \
    -v "$REPO_ROOT:/work" \
    -w /work \
    ubuntu:22.04 \
    bash << 'INNER'
set -e

export DEBIAN_FRONTEND=noninteractive
apt-get update -qq
apt-get install -y -qq --no-install-recommends grub-pc dosfstools fdisk

HDD="/work/$HDD_IMG"
MNT=/mnt/makar-install

# Create a blank raw disk image.
rm -f "$HDD"
dd if=/dev/zero of="$HDD" bs=1M count="$HDD_SIZE_MB" status=none

# Write a single bootable FAT32-LBA (type 0x0C) partition starting at LBA
# 2048 (1 MiB-aligned, leaves room for GRUB's embedding area in sectors 1-2047).
sfdisk "$HDD" << 'PTAB'
label: dos
start=2048, type=c, bootable
PTAB

# Attach two loop devices:
#   LOOP  — whole disk (needed by grub-install to write MBR + embedding area)
#   PART  — partition 1 only, via --offset (avoids --partscan which may not
#            create sub-devices inside Docker containers)
# Partition 1 starts at LBA 2048; each sector is 512 bytes.
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

# Format the partition as FAT32 with the volume label MAKAR.
mkfs.fat -F 32 -n MAKAR "$PART"

# Mount and populate the FAT32 volume.
mkdir -p "$MNT"
mount "$PART" "$MNT"

mkdir -p "$MNT/boot/grub/i386-pc"
cp /work/sysroot/boot/makar.kernel "$MNT/boot/makar.kernel"

cat > "$MNT/boot/grub/grub.cfg" << 'GCFG'
set default=0
set timeout=0

menuentry "Makar OS" {
    multiboot2 /boot/makar.kernel
    boot
}
GCFG

# Install GRUB for i386-pc using grub-mkimage + manual placement.
#
# grub-install probes loop devices via /sys/block and /proc/mounts to
# determine partition UUIDs and embeds a `search --fs-uuid` command in
# core.img.  Inside Docker that probe fails silently and the embedded
# UUID doesn't match, dropping GRUB into rescue mode on first boot.
#
# We avoid grub-install entirely:
#   1. grub-mkimage builds core.img with the explicit prefix
#      (hd0,msdos1)/boot/grub — no device search, no UUID embedded.
#   2. boot.img is written to the first 446 bytes of the disk (MBR),
#      preserving the partition table in bytes 446-511.
#   3. core.img goes into the BIOS Boot / embedding area (sectors 1-2047).
#      boot.img's default kernel-sector pointer is 1, so no further
#      patching is needed.
#   4. All i386-pc GRUB modules are copied to the FAT32 partition so that
#      grub.cfg can load multiboot2.mod and any other modules at runtime.

grub-mkimage \
    -O i386-pc \
    -o /tmp/core.img \
    -p '(hd0,msdos1)/boot/grub' \
    biosdisk part_msdos fat normal multiboot2

# Modules to FAT32 (runtime-loaded by grub.cfg).
cp /usr/lib/grub/i386-pc/*.mod "$MNT/boot/grub/i386-pc/"
cp /usr/lib/grub/i386-pc/*.lst "$MNT/boot/grub/i386-pc/" 2>/dev/null || true
cp /usr/lib/grub/i386-pc/core.img "$MNT/boot/grub/i386-pc/core.img" 2>/dev/null || true

# Write boot.img to MBR (first 446 bytes only — leave partition table intact).
dd if=/usr/lib/grub/i386-pc/boot.img of="$LOOP" bs=1 count=446 conv=notrunc status=none

# Write core.img to the embedding area (sectors 1 onwards).
dd if=/tmp/core.img of="$LOOP" bs=512 seek=1 conv=notrunc status=none

echo "  GRUB installed (grub-mkimage, explicit (hd0,msdos1) prefix)"

umount "$MNT"
rmdir  "$MNT"
losetup -d "$PART"
losetup -d "$LOOP"
trap - EXIT

# Return ownership to the calling user so the file is not root-owned on the host.
chown "${HOST_UID}:${HOST_GID}" "$HDD" 2>/dev/null || true

echo "  Image ready: $HDD"
INNER

echo ""
echo "==> Done. To boot:"
echo ""
printf '    qemu-system-i386 \\\n'
printf '        -drive file=%s,format=raw,if=ide,index=0 \\\n' "$HDD_IMG"
printf '        -serial stdio \\\n'
printf '        -boot c\n'
echo ""
echo "    Add -drive file=makar.iso,if=ide,index=2,media=cdrom if you"
echo "    also want /cdrom accessible from the shell."
