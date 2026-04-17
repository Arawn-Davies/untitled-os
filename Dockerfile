# Dockerfile – Makar OS build environment
#
# Inherits the pre-built i686-elf cross-compiler image used by CI so that the
# local container matches the CI environment exactly.  The image ships:
#   • i686-elf-gcc / binutils cross-toolchain
#   • grub-mkrescue, grub-mkimage, grub-file, xorriso
#   • qemu-system-i386
#   • gdb-multiarch, make
#
# Build the image:
#   docker build -t makar .
#
# Build the ISO (output appears in the bind-mounted source directory):
#   docker run --rm -v "$PWD":/work -w /work makar bash iso.sh
#
# Or use the helper scripts directly:
#   ./docker-iso.sh      – build makar.iso
#   ./docker-qemu.sh     – build then launch QEMU on the host
#   ./docker-test.sh     – build (debug) then run GDB test suite on the host

FROM arawn780/gcc-cross-i686-elf:fast

WORKDIR /work

# Copy the entire source tree into the image.
# When using docker-compose or the docker run -v approach the bind mount will
# overlay this copy, but having a copy baked in allows stand-alone use.
COPY . .

# Default command: build the bootable ISO.
CMD ["bash", "iso.sh"]
