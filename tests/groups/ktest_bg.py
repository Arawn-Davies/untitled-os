"""Background ktest verification group.

Verifies that the background ktest task completed successfully.
Sets a watchpoint on ktest_bg_done and continues until it becomes 1.
"""

import gdb

NAME = 'Background ktest'


def run():
    # Check if already done (may have completed during prior groups).
    try:
        done = int(gdb.parse_and_eval('ktest_bg_done'))
    except gdb.error as exc:
        print('FAIL: could not evaluate ktest_bg_done: ' + str(exc),
              flush=True)
        return False

    if done == 1:
        print('PASS: ktest_bg_done = 1 — background tests completed',
              flush=True)
        print('GROUP PASS: ' + NAME, flush=True)
        return True

    # Not done yet — break when keyboard_getchar is called (shell is waiting
    # for input, which means the ktest wait loop in shell_run has passed).
    bp = gdb.Breakpoint('keyboard_getchar', internal=True, temporary=True)
    bp.silent = True
    try:
        gdb.execute('continue')
    except gdb.error as exc:
        print('FAIL: GDB error waiting for keyboard_getchar: ' + str(exc),
              flush=True)
        return False

    try:
        done = int(gdb.parse_and_eval('ktest_bg_done'))
    except gdb.error as exc:
        print('FAIL: could not evaluate ktest_bg_done: ' + str(exc),
              flush=True)
        return False

    if done != 1:
        print('FAIL: ktest_bg_done = {} (expected 1)'.format(done),
              flush=True)
        return False

    print('PASS: ktest_bg_done = 1 — background tests completed', flush=True)
    print('GROUP PASS: ' + NAME, flush=True)
    return True
