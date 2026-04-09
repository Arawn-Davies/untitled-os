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

The script also verifies the Multiboot 2 magic value in %eax at _start
(0x36D76289) before proceeding with the checkpoint sequence.

After all checkpoints pass, three additional hardware-state checks run:
  1. CR0.PG (bit 31) must be set  – paging was enabled.
  2. CR3 must be non-zero         – page directory is loaded.
  3. timer_callback must fire     – PIT is ticking (kernel is alive).
"""

import gdb  # provided by GDB's embedded Python interpreter

MULTIBOOT2_MAGIC = 0x36D76289

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
    'paging_init',
    'init_serial',
    'init_timer',
    'kernel_post_boot',
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


# ---------------------------------------------------------------------------
# Post-boot hardware and liveness verification
# ---------------------------------------------------------------------------

def run_post_boot_checks():
    """Verify CR0.PG, CR3, and that the PIT timer interrupt is firing.

    Called after all boot checkpoints have been reached.  Execution is
    stopped at the entry of kernel_post_boot (the last checkpoint), so
    ksleep has not yet been called.  When we continue, the PIT fires and
    timer_callback is called, which we catch with a one-shot breakpoint.
    """
    # --- CR0.PG -----------------------------------------------------------
    cr0 = int(gdb.parse_and_eval('$cr0')) & 0xFFFFFFFF
    if cr0 & 0x80000000:
        print('PASS: CR0.PG is set (paging enabled, CR0=0x{:08X})'.format(cr0),
              flush=True)
    else:
        print('FAIL: CR0.PG not set (CR0=0x{:08X})'.format(cr0), flush=True)
        gdb.execute('quit 1')
        return False

    # --- CR3 --------------------------------------------------------------
    cr3 = int(gdb.parse_and_eval('$cr3')) & 0xFFFFFFFF
    if cr3 != 0:
        print('PASS: CR3 is non-zero (page directory at 0x{:08X})'.format(cr3),
              flush=True)
    else:
        print('FAIL: CR3 is zero (page directory not loaded)', flush=True)
        gdb.execute('quit 1')
        return False

    # --- Timer callback ---------------------------------------------------
    # Continuing from kernel_post_boot entry causes ksleep(50) to run,
    # during which the PIT fires at least once and calls timer_callback.
    timer_hit = [False]

    class TimerBreakpoint(gdb.Breakpoint):
        def stop(self):
            timer_hit[0] = True
            try:
                tick = int(gdb.parse_and_eval('tick'))
                print('PASS: timer_callback fired (tick={})'.format(tick),
                      flush=True)
            except gdb.error:
                print('PASS: timer_callback fired', flush=True)
            return True  # stop so we can exit cleanly

    tbp = TimerBreakpoint('timer_callback', internal=True)
    tbp.silent = True
    try:
        gdb.execute('continue')
    except gdb.error as exc:
        print('GDB error waiting for timer_callback: ' + str(exc), flush=True)
    tbp.delete()

    if not timer_hit[0]:
        print('FAIL: timer_callback never fired (PIT not running?)', flush=True)
        gdb.execute('quit 1')
        return False

    return True


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def main():
    gdb.execute('set pagination off')
    gdb.execute('set confirm off')
    gdb.execute('target remote :1234')

    # --- Multiboot 2 magic check -------------------------------------------
    # Break at the very first instruction of the kernel entry point and verify
    # that the bootloader placed the Multiboot 2 magic value in %eax.
    mb2_bp = gdb.Breakpoint('_start', internal=True)
    mb2_bp.silent = True
    try:
        gdb.execute('continue')
    except gdb.error as exc:
        print('GDB error reaching _start: ' + str(exc), flush=True)
        gdb.execute('quit 1')
        return

    eax = int(gdb.parse_and_eval('$eax')) & 0xFFFFFFFF
    print('EAX at _start: 0x{:08X}'.format(eax), flush=True)
    if eax != MULTIBOOT2_MAGIC:
        print(
            'FAIL: expected Multiboot 2 magic 0x{:08X}, got 0x{:08X}'.format(
                MULTIBOOT2_MAGIC, eax),
            flush=True,
        )
        gdb.execute('quit 1')
        return
    print('PASS: Multiboot 2 magic verified (0x{:08X})'.format(eax), flush=True)
    mb2_bp.delete()
    # -----------------------------------------------------------------------

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
        return

    print('ALL CHECKPOINTS PASSED', flush=True)

    # --- Post-boot hardware and liveness verification ----------------------
    if run_post_boot_checks():
        print('ALL POST-BOOT CHECKS PASSED', flush=True)
        gdb.execute('quit 0')


main()
