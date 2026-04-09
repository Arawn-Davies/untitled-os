"""GDB Multiboot 2 magic verification script for untitled-os.

Usage (invoked by the CI workflow or manually):
    gdb-multiarch -batch -ex "source tests/gdb_mb2_verify.py" kernel/untitled-os.kernel

QEMU must be started with -s -S before this script is run so that the GDB
stub is available on localhost:1234 and the CPU is frozen at reset.

The script sets a breakpoint on the kernel entry point (_start).  When the
breakpoint is hit, it reads the value of %eax and checks that it matches the
Multiboot 2 bootloader magic (0x36D76289).  A non-matching value (including
the Multiboot 1 magic 0x2BADB002) causes the script to exit with code 1.
"""

import gdb  # provided by GDB's embedded Python interpreter

MULTIBOOT2_MAGIC = 0x36D76289


def main():
    gdb.execute('set pagination off')
    gdb.execute('set confirm off')
    gdb.execute('target remote :1234')

    # Break at the very first instruction of the kernel entry point.
    bp = gdb.Breakpoint('_start', internal=True)
    bp.silent = True

    try:
        gdb.execute('continue')
    except gdb.error as exc:
        print('GDB error during continue: ' + str(exc), flush=True)
        gdb.execute('quit 1')
        return

    eax = int(gdb.parse_and_eval('$eax')) & 0xFFFFFFFF
    print('EAX at _start: 0x{:08X}'.format(eax), flush=True)

    if eax == MULTIBOOT2_MAGIC:
        print('PASS: Multiboot 2 magic verified (0x{:08X})'.format(eax), flush=True)
        gdb.execute('quit 0')
    else:
        print(
            'FAIL: expected Multiboot 2 magic 0x{:08X}, got 0x{:08X}'.format(
                MULTIBOOT2_MAGIC, eax),
            flush=True,
        )
        gdb.execute('quit 1')


main()
