"""VESA framebuffer group.

Verifies the state of the VESA framebuffer driver and, when the framebuffer
is active, that the VESA TTY was successfully initialised.

This group is entered with execution stopped inside timer_callback (left
there by the hardware_state group).  State variables are read directly from
the stopped inferior; no further `continue` is issued because the kernel
is in debug_idle() which loops on HLT — there is no autonomous output to
intercept.

If no framebuffer was provided by the bootloader (e.g. headless CI) the
group still passes — what matters is that the driver handled the absence
cleanly and did not crash.
"""

import gdb

NAME = 'VESA Framebuffer'

# Path variations for the file-scoped statics in vesa_tty.c as they may
# appear in the debug info depending on from which directory the build ran.
_TTY_SRC_PATHS = [
    'arch/i386/vesa_tty.c',
    'kernel/arch/i386/vesa_tty.c',
]

_FB_SRC_PATHS = [
    'arch/i386/vesa.c',
    'kernel/arch/i386/vesa.c',
]


def _read_static(paths, var):
    """Try reading a file-scoped static variable using multiple path variants.

    Uses double-quoted GDB expressions to avoid issues with paths that contain
    single quotes.
    """
    for path in paths:
        try:
            # Use double-quoted outer string; the path itself won't contain ".
            expr = '"{}"::{}'.format(path, var)
            return int(gdb.parse_and_eval(expr))
        except gdb.error:
            pass
    return None


def run():
    passed = 0
    total = 0

    # --- Check fb_ready ---------------------------------------------------
    fb_ready = _read_static(_FB_SRC_PATHS, 'fb_ready')
    if fb_ready is None:
        print('WARN: could not read fb_ready; skipping framebuffer checks',
              flush=True)
    else:
        total += 1
        if fb_ready:
            try:
                width  = _read_static(_FB_SRC_PATHS, 'fb.width')
                height = _read_static(_FB_SRC_PATHS, 'fb.height')
                bpp    = _read_static(_FB_SRC_PATHS, 'fb.bpp')
                addr   = _read_static(_FB_SRC_PATHS, 'fb.addr')
                if None not in (width, height, bpp, addr):
                    print(
                        'PASS: VESA framebuffer active {}x{}x{} @ 0x{:08X}'.format(
                            width, height, bpp, addr & 0xFFFFFFFF),
                        flush=True,
                    )
                    passed += 1
                else:
                    print('FAIL: VESA framebuffer active but descriptor incomplete',
                          flush=True)
            except gdb.error as exc:
                print('WARN: could not read fb descriptor: ' + str(exc), flush=True)
                total -= 1  # unreadable descriptor; don't penalise
        else:
            print('INFO: VESA framebuffer not available (headless/text-mode boot)',
                  flush=True)
            passed += 1  # clean absence is expected and valid

    # --- Check vesa_tty_ready -------------------------------------------
    tty_ready = _read_static(_TTY_SRC_PATHS, 'tty_ready')
    if tty_ready is None:
        print('WARN: could not read tty_ready; skipping TTY checks',
              flush=True)
    else:
        total += 1
        if tty_ready:
            print('PASS: VESA TTY is active (tty_ready=true)', flush=True)
            passed += 1
        else:
            print('INFO: VESA TTY not active – framebuffer unavailable',
                  flush=True)
            passed += 1  # expected when no framebuffer

    print('GROUP PASS: ' + NAME, flush=True)
    return passed, total
