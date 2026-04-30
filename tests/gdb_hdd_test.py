"""GDB test runner for Makar — HDD boot.

Usage (invoked by run.sh hdd-test):
    gdb-multiarch -batch -ex "source tests/gdb_hdd_test.py" src/kernel/makar.kernel

QEMU must be started with:
    -drive file=makar-hdd-test.img,format=raw,if=ide,index=0
    -boot c  -s -S

The runner:
  1. Connects to a QEMU GDB stub on localhost:1234.
  2. Verifies the Multiboot 2 magic value in %eax at _start — confirms
     GRUB on the HDD image passed the correct magic to the kernel.
  3. Runs each test group in order.

Test groups (all groups run on both ISO and HDD boot paths)
-----------
  boot_checkpoints  – sequential breakpoints through the boot sequence
  hardware_state    – CR0/CR3 paging state and PIT liveness
  vesa              – VESA framebuffer driver state and TTY output-path check
  hdd_mount         – fat32_mounted() non-zero, confirming /hd is up

Adding a new group: create tests/groups/<name>.py with NAME and run(), then
import it into both gdb_boot_test.py and gdb_hdd_test.py.
"""

import os
import sys

try:
    _tests_dir = os.path.dirname(os.path.abspath(__file__))
except NameError:
    _tests_dir = os.path.join(os.getcwd(), 'tests')
sys.path.insert(0, _tests_dir)

import gdb  # noqa: E402
from groups import boot_checkpoints  # noqa: E402
from groups import hardware_state    # noqa: E402
from groups import vesa              # noqa: E402
from groups import hdd_mount         # noqa: E402

MULTIBOOT2_MAGIC = 0x36D76289

GROUPS = [
    boot_checkpoints,
    hardware_state,
    vesa,
    hdd_mount,
]


def check_multiboot2_magic():
    """Break at _start and verify %eax contains the Multiboot 2 magic value."""
    mb2_bp = gdb.Breakpoint('_start', internal=True)
    mb2_bp.silent = True
    try:
        gdb.execute('continue')
    except gdb.error as exc:
        print('GDB error reaching _start: ' + str(exc), flush=True)
        return False

    eax = int(gdb.parse_and_eval('$eax')) & 0xFFFFFFFF
    print('EAX at _start: 0x{:08X}'.format(eax), flush=True)
    mb2_bp.delete()

    if eax != MULTIBOOT2_MAGIC:
        print(
            'FAIL: expected Multiboot 2 magic 0x{:08X}, got 0x{:08X}'.format(
                MULTIBOOT2_MAGIC, eax),
            flush=True,
        )
        return False

    print('PASS: Multiboot 2 magic verified (0x{:08X})'.format(eax), flush=True)
    return True


def main():
    gdb.execute('set pagination off')
    gdb.execute('set confirm off')
    gdb.execute('target remote :1234')

    if not check_multiboot2_magic():
        gdb.execute('quit 1')
        return

    for group in GROUPS:
        print('--- Running group: {} ---'.format(group.NAME), flush=True)
        if not group.run():
            print('GROUP FAIL: ' + group.NAME, flush=True)
            gdb.execute('quit 1')
            return

    print('ALL GROUPS PASSED', flush=True)
    gdb.execute('quit 0')


main()
