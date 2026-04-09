"""GDB boot-test script for untitled-os.

Usage (invoked by the CI workflow):
    gdb-multiarch -batch -ex "source tests/gdb_boot_test.py" kernel/untitled-os.kernel

The script connects to a QEMU GDB stub on localhost:1234 (QEMU must be started
with -s -S before this script is run), sets a breakpoint on every boot
checkpoint, and lets the kernel run.  Each breakpoint auto-continues so that
a single `gdb.execute('continue')` drives the kernel all the way through the
boot sequence.  When the final checkpoint is hit the script calls `quit 0`;
if any checkpoint is missed (or an error occurs) it calls `quit 1`.

Checkpoints are printed as:
    CHECKPOINT: <function-name>
so the CI step can also grep the log for individual names if desired.
"""

import gdb  # provided by GDB's embedded Python interpreter

# ---------------------------------------------------------------------------
# Boot checkpoints – every function listed here must be called during a
# normal kernel boot.  Order must match the actual call order so that the
# last entry triggers the clean shutdown of the test.
# ---------------------------------------------------------------------------
CHECKPOINTS = [
    'kernel_main',
    'terminal_initialize',
    'init_descriptor_tables',
    'init_debug_handlers',
    'pmm_init',
    'init_serial',
    'init_timer',
]

hit = set()


class CheckpointBreakpoint(gdb.Breakpoint):
    """Silent breakpoint that records a hit and auto-continues.

    Returns True (stop) only when *all* checkpoints have been reached so
    that the outer `gdb.execute('continue')` call can return without
    blocking forever in the kernel's post-boot HLT loop.
    """

    def stop(self):
        hit.add(self.location)
        print('CHECKPOINT: ' + self.location, flush=True)
        return hit >= set(CHECKPOINTS)


def main():
    gdb.execute('set pagination off')
    gdb.execute('set confirm off')
    gdb.execute('target remote :1234')

    for fn in CHECKPOINTS:
        bp = CheckpointBreakpoint(fn)
        bp.silent = True

    try:
        # A single continue drives through every auto-continuing breakpoint
        # and only returns when the last checkpoint stops execution.
        gdb.execute('continue')
    except gdb.error as exc:
        print('GDB error during continue: ' + str(exc), flush=True)

    missing_checkpoints = [fn for fn in CHECKPOINTS if fn not in hit]
    if missing_checkpoints:
        for fn in missing_checkpoints:
            print('FAIL: checkpoint not reached: ' + fn, flush=True)
        gdb.execute('quit 1')
    else:
        print('ALL CHECKPOINTS PASSED', flush=True)
        gdb.execute('quit 0')


main()
