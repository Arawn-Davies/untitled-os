#!/usr/bin/env bash
# ui_test.sh - black-box UI tests driven through QEMU's HMP monitor.
#
# Each scenario boots the test ISO headless, feeds keyboard input via
# `sendkey` over the monitor socket, and asserts on substrings in the
# kernel's serial log (mirror of every t_writestring call).  No PPM
# parsing - the serial mirror catches everything any command echoes.
# Cases that depend on visual-only state (cursor position, gutter
# rendering) are out of scope; manual sendkey + screendump for those.
#
# Usage:
#   tests/ui_test.sh                # run all scenarios
#   tests/ui_test.sh <name>...      # run named scenarios only
#
# Exit codes: 0 = all passed, 1 = at least one failed.

set -u

ISO=${ISO:-makar.iso}
if [ ! -f "$ISO" ]; then
    echo "ui_test: $ISO not found - build first with './run.sh iso-build'" >&2
    exit 2
fi

QEMU=${QEMU:-qemu-system-i386}
if ! command -v "$QEMU" >/dev/null 2>&1; then
    echo "ui_test: $QEMU not on PATH; this target needs host qemu" >&2
    exit 2
fi

# Allow CI to capture logs by passing $UI_TEST_LOGDIR; otherwise use a
# scratch dir and clean up on exit.
if [ -n "${UI_TEST_LOGDIR:-}" ]; then
    LOGDIR=$UI_TEST_LOGDIR
    mkdir -p "$LOGDIR"
else
    LOGDIR=$(mktemp -d -t makar-ui)
    trap 'rm -rf "$LOGDIR"' EXIT
fi

# GUI=1 swaps `-display none` for the host's default QEMU window so you can
# *watch* the scenario type itself out.  Override the specific backend with
# QEMU_DISPLAY=cocoa|gtk|sdl when QEMU's default pick is wrong (macOS often
# wants cocoa; X11/Wayland boxes want gtk).
#
# In GUI mode we also pace the sendkey stream with KEY_DELAY (default 0.15 s
# per keystroke) so typing is visible.  Headless runs keep the original
# burst-then-sleep cadence for speed.
GUI=${GUI:-0}
if [ "$GUI" = "1" ]; then
    DISPLAY_ARG=${QEMU_DISPLAY:+-display $QEMU_DISPLAY}
    KEY_DELAY=${KEY_DELAY:-0.15}
    # GUI mode drives a real kernel shutdown via the `shutdown` shell
    # builtin (-> acpi_shutdown -> port 0x604/0x2000 -> QEMU exits on
    # its own), so we must NOT pass -no-shutdown - that flag tells QEMU
    # to pause instead of exit on guest power-off, which would deadlock
    # the wait loop.  -no-reboot stays on so a triple fault still aborts
    # cleanly instead of looping.
    REBOOT_ARG="-no-reboot"
else
    DISPLAY_ARG="-display none"
    KEY_DELAY=${KEY_DELAY:-0}
    REBOOT_ARG="-no-reboot -no-shutdown"
fi

# send_script - feed the HMP monitor a sendkey script, optionally spaced
# with KEY_DELAY so a watching human can see each keystroke land.  The
# socket is short-lived (one nc per line) when paced; this is slower but
# the only way `sendkey` shows up frame-by-frame in the QEMU window.
send_script() {
    local mon=$1
    local script=$2
    if [ "$KEY_DELAY" = "0" ] || [ -z "$KEY_DELAY" ]; then
        nc -U "$mon" <<< "$script" >/dev/null
        return
    fi
    while IFS= read -r line; do
        [ -z "$line" ] && continue
        echo "$line" | nc -U "$mon" >/dev/null
        sleep "$KEY_DELAY"
    done <<< "$script"
}

SERIAL_LOG="$LOGDIR/ui.serial"
MONITOR_SOCK="$LOGDIR/ui.mon"
QEMU_PID=""

stop_qemu() {
    if [ -z "$QEMU_PID" ] || ! kill -0 "$QEMU_PID" 2>/dev/null; then
        return 0
    fi

    if [ "$GUI" = "1" ]; then
        sleep 1
        local shutdown_keys='sendkey s
sendkey e
sendkey h
sendkey u
sendkey t
sendkey d
sendkey o
sendkey w
sendkey n
sendkey ret'
        send_script "$MONITOR_SOCK" "$shutdown_keys"
        local exit_waited=0
        while [ $exit_waited -lt 80 ] && kill -0 "$QEMU_PID" 2>/dev/null; do
            sleep 0.1
            exit_waited=$((exit_waited + 1))
        done
        if kill -0 "$QEMU_PID" 2>/dev/null; then
            echo "WARN: kernel did not ACPI-off; sending HMP quit" >&2
            echo "quit" | nc -U "$MONITOR_SOCK" >/dev/null 2>&1 || true
            exit_waited=0
            while [ $exit_waited -lt 50 ] && kill -0 "$QEMU_PID" 2>/dev/null; do
                sleep 0.1
                exit_waited=$((exit_waited + 1))
            done
        fi
    else
        echo "quit" | nc -U "$MONITOR_SOCK" >/dev/null
        local exit_waited=0
        while [ $exit_waited -lt 50 ] && kill -0 "$QEMU_PID" 2>/dev/null; do
            sleep 0.1
            exit_waited=$((exit_waited + 1))
        done
    fi

    if kill -0 "$QEMU_PID" 2>/dev/null; then
        echo "WARN: QEMU did not exit on shutdown; killing" >&2
        kill -9 "$QEMU_PID" 2>/dev/null
    fi
    wait "$QEMU_PID" 2>/dev/null
    QEMU_PID=""
}

start_qemu() {
    rm -f "$SERIAL_LOG" "$MONITOR_SOCK"

    # shellcheck disable=SC2086
    "$QEMU" \
        -cdrom "$ISO" \
        -m 256 \
        -vga std \
        $DISPLAY_ARG \
        $REBOOT_ARG \
        -serial "file:$SERIAL_LOG" \
        -monitor "unix:$MONITOR_SOCK,server,nowait" \
        &
    QEMU_PID=$!
    trap 'stop_qemu' EXIT

    local boot_timeout=${UI_BOOT_TIMEOUT:-90}
    local waited=0
    local max_ticks=$((boot_timeout * 2))
    while [ $waited -lt $max_ticks ]; do
        if grep -q "kernel: boot complete" "$SERIAL_LOG" 2>/dev/null; then break; fi
        if ! kill -0 "$QEMU_PID" 2>/dev/null; then
            echo "FAIL: QEMU exited before boot-complete" >&2
            return 1
        fi
        sleep 0.5
        waited=$((waited + 1))
    done
    if [ $waited -ge $max_ticks ]; then
        echo "FAIL: boot-complete marker never appeared (waited ${boot_timeout}s)" >&2
        return 1
    fi
    sleep 1

    waited=0
    while [ $waited -lt 30 ] && [ ! -S "$MONITOR_SOCK" ]; do
        sleep 0.2
        waited=$((waited + 1))
    done
    if [ ! -S "$MONITOR_SOCK" ]; then
        echo "FAIL: monitor socket never appeared" >&2
        return 1
    fi

    local preamble='sendkey v
sendkey e
sendkey r
sendkey b
sendkey o
sendkey s
sendkey e
sendkey spc
sendkey o
sendkey n
sendkey ret'
    send_script "$MONITOR_SOCK" "$preamble"
    sleep 0.8
}

run_scenario() {
    local name=$1
    local script=$2
    shift 2
    local expects=("$@")

    local dump=$LOGDIR/$name.ppm
    local segment=$LOGDIR/$name.serial
    rm -f "$dump" "$segment"

    # Reset to a stable baseline (focused VT0, cwd=/cdrom/apps) so each
    # scenario remains independent while reusing the same VM session.
    local reset_script='sendkey alt-f1
sendkey c
sendkey d
sendkey spc
sendkey slash
sendkey c
sendkey d
sendkey r
sendkey o
sendkey m
sendkey slash
sendkey a
sendkey p
sendkey p
sendkey s
sendkey ret'
    send_script "$MONITOR_SOCK" "$reset_script"
    sleep 0.6

    local start_bytes=0
    if [ -f "$SERIAL_LOG" ]; then
        start_bytes=$(wc -c < "$SERIAL_LOG")
    fi

    send_script "$MONITOR_SOCK" "$script"
    sleep 3

    echo "screendump $dump" | nc -U "$MONITOR_SOCK" >/dev/null
    sleep 0.3

    local start_pos=$((start_bytes + 1))
    tail -c +"$start_pos" "$SERIAL_LOG" > "$segment"

    # Assert every expected substring appears in serial.
    local missing=()
    for needle in "${expects[@]}"; do
        if ! grep -qF -- "$needle" "$segment"; then
            missing+=("$needle")
        fi
    done

    if [ ${#missing[@]} -eq 0 ]; then
        echo "PASS [$name]"
        return 0
    else
        echo "FAIL [$name]: missing in serial: ${missing[*]}"
        echo "       serial: $segment"
        echo "       dump:   $dump"
        return 1
    fi
}

# --- Scenarios ---------------------------------------------------------------
#
# Each scenario is named; selecting a subset on the command line filters by
# name.  Scripts are HMP `sendkey` commands; the framework appends a `ret` if
# the script doesn't already end with one - reduces boilerplate.

scenario_glob_proc() {
    run_scenario "glob-proc" \
"sendkey c
sendkey a
sendkey t
sendkey spc
sendkey slash
sendkey p
sendkey r
sendkey o
sendkey c
sendkey slash
sendkey shift-8
sendkey ret" \
        "vendor_id" "MemFree" "Makar 0.5.0"
}

scenario_tab_path() {
    # `cat<TAB>/proc/c<TAB><Enter>` should expand to `cat /proc/cpuinfo`
    # and dump the cpuinfo content.  Verifies: visible-feedback tab on
    # unique cmd match (`cat ` with trailing space), path-style tab
    # completion extension, and that the resulting command runs.
    run_scenario "tab-complete-path" \
"sendkey c
sendkey a
sendkey t
sendkey tab
sendkey slash
sendkey p
sendkey r
sendkey o
sendkey c
sendkey slash
sendkey c
sendkey tab
sendkey ret" \
        "vendor_id" "GenuineIntel"
}

scenario_exec_hello() {
    # `exec apps/hello.elf tester` prints "Hello, tester!\n" via sys_write
    # on fd 2 (stderr = FD_KIND_VGA_SERIAL).  Verifies the per-task fd
    # table end-to-end: elf_exec spawns a child task, task_create allocates
    # its fd_table with fds 0/1/2 pre-bound, sys_write dispatches to
    # serial through fd_get on the calling task's table.  Pre-#134 this
    # path went through the global s_fds[]; if fd-table allocation or
    # lookup regresses, the greeting never reaches COM1.
    #
    # Path is relative to /cdrom (the auto-CWD on CD-only boots) so the
    # apps directory resolves without a /cdrom prefix.
    run_scenario "exec-hello" \
"sendkey e
sendkey x
sendkey e
sendkey c
sendkey spc
sendkey a
sendkey p
sendkey p
sendkey s
sendkey slash
sendkey h
sendkey e
sendkey l
sendkey l
sendkey o
sendkey dot
sendkey e
sendkey l
sendkey f
sendkey spc
sendkey t
sendkey e
sendkey s
sendkey t
sendkey e
sendkey r
sendkey ret" \
        "Hello," "tester" "status=0"
}

scenario_per_tty_cwd() {
    # Per-task cwd isolation across TTYs (slice 15).
    #
    # Layout: each shell0..3 task owns task_t.cwd; vfs_getcwd/vfs_cd route
    # through task_current()->cwd.  We:
    #   1. On VT0 (default focus): cd /proc          -> prompt becomes "/proc~>"
    #   2. Alt+F2 → VT1, cd /cdrom/apps               -> "/cdrom/apps~>"
    #   3. Alt+F1 → VT0 (re-render its prompt)        -> still "/proc~>"
    # The shell re-prints its prompt on every focus switch (per-TTY backing
    # grid replays into the framebuffer), so seeing /proc~> appear in serial
    # AFTER /cdrom/apps~> is direct evidence that VT0's cwd survived VT1's
    # excursion - which is exactly what slice 15 guarantees.  Asserting on
    # the "~>" suffix makes the matches unambiguous (only prompts end that
    # way; raw command echo does not).
    run_scenario "per-tty-cwd" \
"sendkey c
sendkey d
sendkey spc
sendkey slash
sendkey p
sendkey r
sendkey o
sendkey c
sendkey ret
sendkey alt-f2
sendkey c
sendkey d
sendkey spc
sendkey slash
sendkey c
sendkey d
sendkey r
sendkey o
sendkey m
sendkey slash
sendkey a
sendkey p
sendkey p
sendkey s
sendkey ret
sendkey alt-f1" \
        "/proc~>" "/cdrom/apps~>"
}

scenario_cd_root() {
    # `cd /<TAB><TAB><Enter>` then `pwd` - tab on /<TAB><TAB> lists the
    # mount points; the trailing Enter commits the half-typed `cd /`,
    # leaving us at the virtual root.  Then verify with pwd.
    run_scenario "cd-root-listing" \
"sendkey c
sendkey d
sendkey spc
sendkey slash
sendkey tab
sendkey tab
sendkey ret
sendkey p
sendkey w
sendkey d
sendkey ret" \
        "cdrom"
}

scenario_calc_brackets() {
    # Bracket/parenthesis arithmetic smoke test in calc.elf.
    # Uses HMP sendkey and captures a screendump via run_scenario().
    run_scenario "calc-brackets" \
"sendkey e
sendkey x
sendkey e
sendkey c
sendkey spc
sendkey a
sendkey p
sendkey p
sendkey s
sendkey slash
sendkey c
sendkey a
sendkey l
sendkey c
sendkey dot
sendkey e
sendkey l
sendkey f
sendkey ret
sendkey shift-9
sendkey 2
sendkey shift-8
sendkey 3
sendkey shift-0
sendkey shift-8
sendkey 4
sendkey ret
sendkey 6
sendkey 9
sendkey minus
sendkey shift-9
sendkey 6
sendkey shift-8
sendkey shift-9
sendkey 9
sendkey minus
sendkey 1
sendkey shift-0
sendkey shift-0
sendkey ret
sendkey shift-9
sendkey shift-9
sendkey 8
sendkey shift-8
sendkey 2
sendkey shift-0
sendkey shift-8
sendkey shift-9
sendkey 3
sendkey minus
sendkey 1
sendkey shift-0
sendkey shift-0
sendkey ret
sendkey e
sendkey x
sendkey i
sendkey t
sendkey ret" \
        "24" "21" "32"
}

# --- Driver ------------------------------------------------------------------

ALL_SCENARIOS=(glob_proc tab_path exec_hello cd_root per_tty_cwd calc_brackets)

declare -a TO_RUN
if [ $# -eq 0 ]; then
    TO_RUN=("${ALL_SCENARIOS[@]}")
else
    for arg in "$@"; do
        TO_RUN+=("${arg//-/_}")
    done
fi

fails=0
total=0
if ! start_qemu; then
    exit 1
fi
for s in "${TO_RUN[@]}"; do
    total=$((total + 1))
    if ! "scenario_${s}"; then
        fails=$((fails + 1))
    fi
done
stop_qemu

echo
echo "ui_test: $((total - fails))/$total passed"
[ $fails -eq 0 ] || exit 1
exit 0
