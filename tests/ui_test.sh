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

run_scenario() {
    local name=$1
    local script=$2
    shift 2
    local expects=("$@")

    local serial=$LOGDIR/$name.serial
    local mon=$LOGDIR/$name.mon
    local dump=$LOGDIR/$name.ppm

    rm -f "$serial" "$mon" "$dump"

    # -no-reboot stops a triple fault from looping the boot.  -no-shutdown
    # is *only* set in headless mode (see DISPLAY_ARG/REBOOT_ARG above) -
    # GUI mode lets the kernel's ACPI shutdown actually exit QEMU.
    # $DISPLAY_ARG and $REBOOT_ARG are unquoted on purpose so they
    # word-split into the right number of args.
    # shellcheck disable=SC2086
    "$QEMU" \
        -cdrom "$ISO" \
        -m 256 \
        -vga std \
        $DISPLAY_ARG \
        $REBOOT_ARG \
        -serial "file:$serial" \
        -monitor "unix:$mon,server,nowait" \
        &
    local pid=$!

    # Wait up to ${BOOT_TIMEOUT:-90}s for "kernel: boot complete".  TCG
    # under CI containers can take 30-60s to reach this marker on its
    # own; bump UI_BOOT_TIMEOUT in the workflow if you see false fails.
    local boot_timeout=${UI_BOOT_TIMEOUT:-90}
    local waited=0
    local max_ticks=$((boot_timeout * 2))   # 0.5s ticks
    while [ $waited -lt $max_ticks ]; do
        if grep -q "kernel: boot complete" "$serial" 2>/dev/null; then break; fi
        if ! kill -0 "$pid" 2>/dev/null; then
            echo "FAIL [$name]: QEMU exited before boot-complete" >&2
            return 1
        fi
        sleep 0.5
        waited=$((waited + 1))
    done
    if [ $waited -ge $max_ticks ]; then
        echo "FAIL [$name]: boot-complete marker never appeared (waited ${boot_timeout}s)" >&2
        kill -9 "$pid" 2>/dev/null; wait "$pid" 2>/dev/null
        return 1
    fi
    # Small grace period for the shell tasks (created after the marker)
    # to register their kbd slots and print their prompts.
    sleep 1

    # Wait for monitor socket.
    waited=0
    while [ $waited -lt 30 ] && [ ! -S "$mon" ]; do
        sleep 0.2
        waited=$((waited + 1))
    done

    # Prepend `verbose on` so the shell mirrors output to COM1 for the
    # duration of the test.  By default (Linux-style) the shell only
    # writes to the framebuffer, which the serial log wouldn't see.
    # The verbose toggle itself writes its confirmation to serial only
    # AFTER the flag flips, so we can also assert that the toggle ran.
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
    send_script "$mon" "$preamble"
    sleep 0.8

    # Drive scenario keystrokes.
    send_script "$mon" "$script"

    # Let commands drain.  Three seconds covers any current scenario;
    # bump if a test starts running anything slow.
    sleep 3

    # Snapshot the screen (handy for triage; not asserted on here).
    echo "screendump $dump" | nc -U "$mon" >/dev/null
    sleep 0.3

    # Shutdown.  GUI mode types `shutdown<Enter>` into the focused shell
    # so the kernel runs its real ACPI S5 path (port 0x604 / 0x2000),
    # which both exercises that code path and gives the watcher a visible
    # "Shutting down..." final frame before the window closes naturally.
    # Headless mode skips straight to HMP `quit` for speed and to avoid
    # depending on shell-task responsiveness during regression runs.
    if [ "$GUI" = "1" ]; then
        # Brief dwell so the watcher sees the screendump-final state
        # before the shutdown command starts typing.
        sleep 1
        local shutdown_keys='sendkey s
sendkey h
sendkey u
sendkey t
sendkey d
sendkey o
sendkey w
sendkey n
sendkey ret'
        send_script "$mon" "$shutdown_keys"
        # ACPI S5 should drop QEMU within a couple seconds on TCG.
        local exit_waited=0
        while [ $exit_waited -lt 80 ] && kill -0 "$pid" 2>/dev/null; do
            sleep 0.1
            exit_waited=$((exit_waited + 1))
        done
        # Fall back to HMP quit if the kernel didn't power off (wedged
        # shell, ACPI regression, etc).
        if kill -0 "$pid" 2>/dev/null; then
            echo "WARN [$name]: kernel did not ACPI-off; sending HMP quit" >&2
            echo "quit" | nc -U "$mon" >/dev/null 2>&1 || true
            exit_waited=0
            while [ $exit_waited -lt 50 ] && kill -0 "$pid" 2>/dev/null; do
                sleep 0.1
                exit_waited=$((exit_waited + 1))
            done
        fi
    else
        echo "quit" | nc -U "$mon" >/dev/null

        # Bounded shutdown.  If QEMU doesn't honour the HMP `quit` within
        # 5s (monitor socket lost, kernel wedged, etc.) escalate to SIGKILL
        # so a single hang can't burn the whole CI minute budget.
        local exit_waited=0
        while [ $exit_waited -lt 50 ] && kill -0 "$pid" 2>/dev/null; do
            sleep 0.1
            exit_waited=$((exit_waited + 1))
        done
    fi

    if kill -0 "$pid" 2>/dev/null; then
        echo "WARN [$name]: QEMU did not exit on shutdown; killing" >&2
        kill -9 "$pid" 2>/dev/null
    fi
    wait "$pid" 2>/dev/null

    # Assert every expected substring appears in serial.
    local missing=()
    for needle in "${expects[@]}"; do
        if ! grep -qF -- "$needle" "$serial"; then
            missing+=("$needle")
        fi
    done

    if [ ${#missing[@]} -eq 0 ]; then
        echo "PASS [$name]"
        return 0
    else
        echo "FAIL [$name]: missing in serial: ${missing[*]}"
        echo "       serial: $serial"
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

# --- Driver ------------------------------------------------------------------

ALL_SCENARIOS=(glob_proc tab_path exec_hello cd_root)

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
for s in "${TO_RUN[@]}"; do
    total=$((total + 1))
    if ! "scenario_${s}"; then
        fails=$((fails + 1))
    fi
done

echo
echo "ui_test: $((total - fails))/$total passed"
[ $fails -eq 0 ] || exit 1
exit 0
