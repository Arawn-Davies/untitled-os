"""GDB test runner for Makar.

Usage (invoked by the CI workflow):
    gdb-multiarch -batch -ex "source tests/gdb_boot_test.py" kernel/makar.kernel

The runner:
  1. Connects to a QEMU GDB stub on localhost:1234 (QEMU must be started
     with -s -S before this script is run).
  2. Verifies the Multiboot 2 magic value in %eax at _start.
  3. Runs each test group in order.  Groups are defined in tests/groups/ and
     each exposes a NAME string and a run() → (passed: int, total: int) function.
  4. Prints a final RESULTS line with the aggregate pass rate.
  5. Sends 'monitor quit' to the QEMU monitor to shut down the VM, then exits
     GDB with code 0 (all passed) or 1 (any failure).

Test groups
-----------
  boot_checkpoints  – sequential function breakpoints through the boot sequence
  hardware_state    – CR0/CR3 paging state and PIT liveness (timer_callback)
  vesa              – VESA framebuffer driver state and TTY output-path check

Adding a new group: create tests/groups/<name>.py with NAME and run(), then
import it here and append it to GROUPS.  run() must return (passed, total).
"""

import os
import sys

# Ensure the tests/ directory is on the path so the groups sub-package can be
# imported regardless of the working directory GDB was launched from.
try:
    _tests_dir = os.path.dirname(os.path.abspath(__file__))
except NameError:
    _tests_dir = os.path.join(os.getcwd(), 'tests')
sys.path.insert(0, _tests_dir)

import gdb  # noqa: E402  (provided by GDB's embedded Python interpreter)
from groups import boot_checkpoints  # noqa: E402
from groups import hardware_state    # noqa: E402
from groups import vesa              # noqa: E402

MULTIBOOT2_MAGIC = 0x36D76289

GROUPS = [
    boot_checkpoints,
    hardware_state,
    vesa,
]


def check_multiboot2_magic():
    """Break at _start and verify %eax contains the Multiboot 2 magic value.

    Returns (passed, total) where total is always 1.
    """
    mb2_bp = gdb.Breakpoint('_start', internal=True)
    mb2_bp.silent = True
    try:
        gdb.execute('continue')
    except gdb.error as exc:
        print('GDB error reaching _start: ' + str(exc), flush=True)
        return 0, 1

    eax = int(gdb.parse_and_eval('$eax')) & 0xFFFFFFFF
    print('EAX at _start: 0x{:08X}'.format(eax), flush=True)
    mb2_bp.delete()

    if eax != MULTIBOOT2_MAGIC:
        print(
            'FAIL: expected Multiboot 2 magic 0x{:08X}, got 0x{:08X}'.format(
                MULTIBOOT2_MAGIC, eax),
            flush=True,
        )
        return 0, 1

    print('PASS: Multiboot 2 magic verified (0x{:08X})'.format(eax), flush=True)
    return 1, 1


def _shutdown_qemu():
    """Ask QEMU to quit via the GDB monitor interface."""
    try:
        gdb.execute('monitor quit')
    except gdb.error:
        pass  # not fatal – the CI script will kill QEMU if needed


def main():
    gdb.execute('set pagination off')
    gdb.execute('set confirm off')
    gdb.execute('target remote :1234')

    total_passed = 0
    total_tests = 0
    all_passed = True

    mb2_passed, mb2_total = check_multiboot2_magic()
    total_passed += mb2_passed
    total_tests += mb2_total

    if mb2_passed < mb2_total:
        all_passed = False
        print('RESULTS: {}/{} tests passed'.format(total_passed, total_tests),
              flush=True)
        _shutdown_qemu()
        gdb.execute('quit 1')
        return

    for group in GROUPS:
        print('--- Running group: {} ---'.format(group.NAME), flush=True)
        g_passed, g_total = group.run()
        total_passed += g_passed
        total_tests += g_total
        if g_passed < g_total:
            print('GROUP FAIL: ' + group.NAME, flush=True)
            all_passed = False
            # Stop here: later groups depend on CPU state left by earlier ones.
            break

    print('RESULTS: {}/{} tests passed'.format(total_passed, total_tests),
          flush=True)

    _shutdown_qemu()

    if all_passed:
        print('ALL GROUPS PASSED', flush=True)
        gdb.execute('quit 0')
    else:
        gdb.execute('quit 1')


main()

