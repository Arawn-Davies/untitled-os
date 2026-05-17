---
title: History
nav_order: 7
---

# Changelog

Reverse-chronological log of shipped work.  Dates are merge/commit
dates; PR numbers link to the merge that shipped each milestone.
Sections under each release: **Added / Changed / Fixed / Removed**.

For what's queued up next, see the [roadmap](roadmap.md);
this file is the trail of how the current state got there.

## Unreleased

### Added
- **makbox** — Makar busybox-style multicall binary
  (`src/userspace/makbox.c`).  Applets: `ls`, `cat`, `cp`, `mv`, `rm`,
  `rmdir`, `echo`, `pwd`.  Shell dispatch falls back to `makbox.elf
  <name>` after PATH lookup misses, so bare `ls` / `cat` etc. still
  work without symlinks (FAT32 has none).
- **`SYS_GETCWD` (215)** — copies `task_current()->cwd` into a user
  buffer.  Lets makbox `pwd` work without argv injection.  Userspace
  wrapper in `src/userspace/syscall.h`.
- **`task_t.exec_params`** — per-task heap-allocated `exec_params_t`
  pointer set by `shell_exec_elf` and consumed by `exec_task_entry`.
- **`tests/ui_runner.sh`** — shared test-runner framework split out of
  `ui_test.sh`.  Provides `start_qemu` / `stop_qemu` / `it` /
  `assert_serial_contains` / `assert_serial_not_contains` /
  `run_test`.  `PAUSE <secs>` directive in `send_script` lets a single
  test script sleep mid-stream so child tasks (calc) are ready before
  the next batch of keys lands.
- New `test_makbox_pwd` scenario asserts a `[makbox:pwd]` provenance
  tag emitted via `SYS_WRITE_SERIAL` — proves the ring-3 path ran
  end-to-end (not a stale builtin).

### Changed
- Shell dispatch in `shell.c` gains the makbox fallback after PATH
  lookup; first-token tab completion advertises makbox applets.
- `ls`/`cat`/`cp` shell builtins removed from `fs_cmds[]`; tidied
  `shell_cmd_fs.c` comments throughout.
- `tests/ui_test.sh` now reuses one QEMU session across all tests
  (Ctrl+C / `cd /` reset between scenarios) — ~10× faster than the
  per-scenario boot model.  Headless: **7/7 in ~33 s**.  GUI mode
  preserved.  30 ms per-key pacing under TCG so bursts don't out-run
  the kernel's PS/2 ring + readline pipeline.
- GRUB interactive timeout dropped 5 s → 3 s (`iso.sh`,
  `generate-hdd.sh`).

### Fixed
- **Cross-TTY `exec` race** that could land a child task at a garbage
  EIP.  Previously `shell_exec_elf` stashed `path`/`argc`/`argv` in
  file-static globals and handed them off to the child via the static
  address.  Two shells exec'ing concurrently from different TTYs
  would clobber each other; one observed crash:
  `panic(cpu 0): PAGE FAULT … CS=0x3F8 EIP=0xE30 PROT|READ|USER`.
  Per-task `exec_params` closes the race — `exec_task_entry` reads
  from its own task's slot, never a shared static.  Reaped on slot
  reuse.
- **Stale `g_sigint` after Ctrl+C** — `shell_readline` now drains the
  flag when handling `KEY_CTRL_C`.  Previously a buffered SIGINT
  could leak into the next `shell_exec_elf`, killing the child task
  immediately on its first iteration so makbox/exec invocations
  produced no output and the shell just reprinted `^C`.

### Removed
- Standalone `ls.elf` / `echo.elf` / `rm.elf` / `mv.elf` / `cp.elf`
  (now applets in `makbox.elf`).
- `src/kernel/arch/i386/shell/shell_cmd_fileops.c` (rm/rmdir/mv now in
  makbox).

## 0.5.0 — 2026-05-14

[#129](https://github.com/Arawn-Davies/Makar/pull/129).  First tagged
release.

### Added
- Per-TTY `vt_buf_t` backing grids.  Writes go to the grid first; the
  framebuffer is only painted when that TTY is focused.  Background
  TTYs accumulate output without contention.
- tmux-style status bar at the reserved bottom row.
- Synthetic `/proc` filesystem with `cpuinfo`, `meminfo`, `tasks`,
  `uname` as read-only files generated on demand.
- Glob (`*`, `?`) and cross-FS tab completion via `vfs_complete`.
- `MAKAR_VERSION` single-source build constant.

### Changed
- Alt+F1–F4 switch repaints via deferred FB drain
  (`vtty_drain_pending`), out of IRQ context.

## 2026-05-13 — Test infrastructure

### Changed
- ccache toolchain image (`makar-build:local`).  Warm rebuilds ~3×
  faster (16.9 s cold → 5.6 s warm).
- Single-kernel/two-ISO emit (`makar.iso` interactive, `makar-test.iso`
  CI test_mode).  KERNEL_ARGS injection lands here too.
- Build-once fan-out CI: 4 parallel jobs (ktest, gdb-iso, gdb-hdd,
  ui-test).
- KVM gated behind `MAKAR_USE_KVM=1`, off by default (CI reproducibility).

[#125](https://github.com/Arawn-Davies/Makar/pull/125).

## 2026-05-12 — Keyboard hardening

### Added
- Typematic-repeat filter for modifier keys.
- PS/2 LED sync (`0xED <bitmap>` → controller); boot-time LED state read.

### Fixed
- `unsigned char` audit complete across the dispatch path — no more
  sign-extension hazards on sentinel compares.

[#127](https://github.com/Arawn-Davies/Makar/pull/127).

## 2026-05-12 — Keyboard rewrite

### Changed
- Full PS/2 set-1 (+ 0xE0 prefix) decoder.  Layered pipeline:
  scancode → keycode → ASCII/sentinel → router.
- IRQ-driven per-TTY SPSC rings (up to `KB_TASK_SLOTS=4`), strict
  make/break separation, modifier state held at the decoder.
- Sentinels (`0x80`–`0x96`) for arrows, F-keys, modifier events.

### Added
- `kbtester.elf` — live ring-3 diagnostic dumping every event
  (scancode/keycode/sentinel/modifier) to serial.

[#124](https://github.com/Arawn-Davies/Makar/pull/124).

## 2026-05-08 — Preemptive tasking

### Added
- Preemptive 100 Hz scheduler (PIT IRQ 0 yields every
  `SCHED_QUANTUM=4` ticks).
- Per-task `task_t` plumbing — `pid`, `cwd`, `tty`, signal bitmasks,
  `fd_table` placeholder.
- `SYS_WRITE_SERIAL` (211).
- Humanised `uptime` builtin.

### Fixed
- Dead-task user-PD reaper (deferred free on slot reuse) closes a UAF
  that occurred when a timer IRQ landed inside `vmm_free_pd`.

[#123](https://github.com/Arawn-Davies/Makar/pull/123).

## 2026-05-02 — FAT32 userspace fileops

### Added
- Syscalls 208–210: `SYS_DELETE_FILE`, `SYS_RENAME_FILE`,
  `SYS_DELETE_DIR`.
- Shell builtins `rm` / `rmdir` / `mv` and ring-3 apps `rm.elf` /
  `mv.elf` / `cp.elf`.

[#120](https://github.com/Arawn-Davies/Makar/pull/120).

## 2026-05-02 — Multi-TTY

### Added
- Four independent shell tasks (`shell0`–`shell3`), Alt+F1–F4 to
  switch.
- Pane-aware VICS editor.
- `lsman` / `man <cmd>` replace `help`.

[#119](https://github.com/Arawn-Davies/Makar/pull/119).

## 2026-05-01 — Userspace

### Added
- Linux i386 ABI userspace.  Ring-3 protected mode via `iret`, ELF
  loader (`elf_exec`) with argc/argv, syscall surface.
- `exec` shell command.

[#118](https://github.com/Arawn-Davies/Makar/pull/118).

## 2026-05-01 — Shell polish

### Added
- GRUB menu (default Makar OS, fallback to next bootable device).
- 720p VESA default (`-vga std`).
- Ctrl+C sigint, tab completion, `calc.elf`.

[#117](https://github.com/Arawn-Davies/Makar/pull/117).

## 2026-04-30 — Build consolidation

### Changed
- All build/test/boot operations now go through `run.sh` (single
  entrypoint).  Replaces the prior tangle of docker-compose targets
  and manual qemu invocations.

[#116](https://github.com/Arawn-Davies/Makar/pull/116).

## 2026-04-29 — HDD path + auto-release

### Added
- Installed-HDD boot path: image generation
  (`generate-hdd.sh`), interactive boot, GDB test.
- Auto-release workflow on `main` merge.

[#114](https://github.com/Arawn-Davies/Makar/pull/114),
[#115](https://github.com/Arawn-Davies/Makar/pull/115).

## 2026-04-29 — VESA panes

### Added
- VESA pane abstraction (`vesa_pane_t`) — phase 1 of split-pane support.

[#113](https://github.com/Arawn-Davies/Makar/pull/113).

## 2026-04-26 — Display & I/O

### Added
- Panic screen with register dump + boot-screen logo.
- Readline inline editing.
- File-I/O builtins.
- Ring-3 stdin support.
- `exec` waits for the child task before returning to the prompt.

[#110](https://github.com/Arawn-Davies/Makar/pull/110),
[#111](https://github.com/Arawn-Davies/Makar/pull/111),
[#112](https://github.com/Arawn-Davies/Makar/pull/112).

## 2026-04-25 — VMM + ring-3

### Added
- Per-task page directories (`vmm_create_pd`, `vmm_map_page`,
  `vmm_switch`).
- Ring-3 entry trampoline (`ring3_enter`).
- Comprehensive ktest suites for memory + ring-3 prereqs.

### Fixed
- Cross-platform build under macOS arm64 (Docker image targeting).

[#53](https://github.com/Arawn-Davies/Makar/pull/53).

## 2026-04-12 — Filesystem + name

### Added
- FAT32 + Medli filesystem layout (`#26`/`#47`).
- Universal VFS layer with auto-mount.
- `↑`/`↓` shell history.
- Installer.
- Build-timestamp banner.

### Changed
- Project name takes shape — shell vocabulary deliberately mirrors
  Medli (`#48`).
- Docker image swapped for the i686-elf cross-compiler.
- Shell split into focused source files.

## 2026-04-11 — Multitasking + storage

### Added
- Cooperative multitasking + `int 0x80` syscall stub
  ([#42](https://github.com/Arawn-Davies/Makar/pull/42)).
- ATA PIO IDE driver
  ([#41](https://github.com/Arawn-Davies/Makar/pull/41)).
- MBR + GPT partition probe + `mkpart`
  ([#45](https://github.com/Arawn-Davies/Makar/pull/45)).

### Changed
- Source tree reshuffle: kernel moves under `src/kernel/arch/i386/`.

## 2026-04-12 — Memory + ACPI

### Added
- Paging cleanup, ACPI support, on-demand ktest harness
  ([#44](https://github.com/Arawn-Davies/Makar/pull/44)).

## 2026-04-09 — Rebirth

### Changed
- Project rename to "Untitled OS".  GitHub Actions wired up
  (push triggers, Copilot-assisted PR cadence #1–#17).  The build
  acquires real CI, the kernel acquires real structure.

## 2025-01 — Single-commit hiatus

A drive-by "fix assembly comments." commit while the rest of the
codebase slept.

> The project then sat largely dormant from 2020 through early 2026.

## 2020-08 — A brief return

### Changed
- Editorconfig, code-style consolidation.
- Switch from GAS-style inline assembly to standalone NASM files.

Nothing shipped beyond scaffolding.  Repo went quiet again after about
a week.

## 2019-08 — First strokes

### Added
- First commit; display driver newline fix; libc stubs.
- IDT, GDT, basic IRQ handling within the week.
- Serial debug interface.

The project was unnamed and the build was driven by a hand-rolled QEMU
shell script.  Repo went quiet after about a week.

## Roadmap

What's tracked but not shipped — see [roadmap](roadmap.md) for
canonical status and the issue tracker for everything else.

- **Slice 8** — Linux-style signal subsystem (sigaction, `kill()`).
- **Slice 9** — Preemption hardening (interrupt-safe `schedule()`).
- **Slice 16** — VGA-text fallback per-TTY backing buffers.

Long term: musl libc port → dash → in-kernel TCC for write-compile-run
on bare metal.  See [Userland libc](userland-libc.md).
