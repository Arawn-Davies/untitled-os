---
title: Roadmap
nav_order: 8
---

# Roadmap

Where work stands today (May 2026) and what's queued up next.  The two
sources of truth are CLAUDE.md's "Slice queue" (kernel work units) and
the GitHub issue tracker (everything else).  This page is the map
between them.

For the chronological log of what's *already shipped*, see
[history](history.md).

## Slice queue

Kernel/userspace work tracked as numbered slices in CLAUDE.md.  Each
slice is a focused, mergeable unit — usually one PR.

| Slice | Title                                                  | Status                      | Issue                                                                              |
|------:|--------------------------------------------------------|-----------------------------|------------------------------------------------------------------------------------|
|     1 | Reaper for dead-task user PDs                          | ✅ shipped (`fcb8771`)      | —                                                                                  |
|     2 | Per-task `task_t` plumbing                             | ✅ shipped (`3a0ef78`)      | —                                                                                  |
|     3 | Ring-3 lifecycle ktest                                 | ✅ shipped (`f48d730`)      | —                                                                                  |
|     4 | 100 Hz timer + `SYS_WRITE_SERIAL`                      | ✅ shipped (`5e40001`)      | —                                                                                  |
|     5 | Keyboard rewrite (layered decoder)                     | ✅ shipped (#124)           | —                                                                                  |
|    5b | Keyboard hardening                                     | ✅ shipped (#127)           | —                                                                                  |
|     6 | Test-infra cleanup (ccache, fan-out CI)                | ✅ shipped (#125)           | —                                                                                  |
|     7 | Per-task consumer migration                            | ✅ complete                 | —                                                                                  |
|     8 | Linux-style signal subsystem                           | ⏭ queued                    | [#144](https://github.com/Arawn-Davies/Makar/issues/144)                          |
|     9 | Preemption hardening                                   | ⏭ queued                    | [#145](https://github.com/Arawn-Davies/Makar/issues/145)                          |
|    10 | Per-TTY screen buffers + status bar + `/proc`          | ✅ shipped (#129)           | —                                                                                  |
|    11 | `ps`-style task listing                                | ⏭ queued                    | [#147](https://github.com/Arawn-Davies/Makar/issues/147)                          |
|    12 | `fork()` readiness (CoW PD clone, fd dup)              | ⏭ queued                    | rolled into [#121](https://github.com/Arawn-Davies/Makar/issues/121)              |
|    13 | UTF-8 terminal                                         | ⏭ deferred                  | [#148](https://github.com/Arawn-Davies/Makar/issues/148)                          |
|    14 | Per-task FD table                                      | ✅ shipped (#134)           | —                                                                                  |
|    15 | VFS `task->cwd` authoritative                          | ✅ shipped (#135)           | —                                                                                  |
|    16 | VGA-text fallback per-TTY                              | ⏭ queued                    | [#146](https://github.com/Arawn-Davies/Makar/issues/146)                          |
|    17 | makbox multicall + `SYS_GETCWD` + exec race fix        | ✅ shipped (#137)           | —                                                                                  |

## Active themes

Higher-level groupings that cover several slices and/or issues.

### Userspace + libc

Long-term goal: a self-hosting userspace.  Get a real libc on Makar,
then port `dash` (or extend the in-kernel shell), then bring up TCC
for in-place compile-run.  Detailed plan in
[Userland libc](userland-libc.md).

Open: [#121 — process model & libc](https://github.com/Arawn-Davies/Makar/issues/121)
(rolls slice 12's `fork()` work in with musl/uClibc-ng porting).

### Networking

NIC driver → lwIP → DHCP/DNS → simple HTTP/Telnet daemons.

Open:
- [#122 — RTL8139 + lwIP + DHCP/DNS](https://github.com/Arawn-Davies/Makar/issues/122)
  — full stack epic.
- [#141 — IP/TCP over NE2000 or virtio-net](https://github.com/Arawn-Davies/Makar/issues/141)
  — alternative driver target (NE2000/virtio) for the same stack.

### Makar × Medli interop

Sharing data + binaries with the Medli project.

Open:
- [#138 — COM loader (flat DOS-style binaries)](https://github.com/Arawn-Davies/Makar/issues/138)
- [#139 — Serial shell over COM1](https://github.com/Arawn-Davies/Makar/issues/139)
- [#140 — MXF executable format (joint binary format)](https://github.com/Arawn-Davies/Makar/issues/140)
- [#142 — Login + user accounts (`System\Data\usrinfo.sys`)](https://github.com/Arawn-Davies/Makar/issues/142)
- [#143 — INI-style config and data exchange](https://github.com/Arawn-Davies/Makar/issues/143)

Background: [Makar × Medli](makar-medli.md).

### Kernel hardening

Things the kernel needs to be properly preemptive + signal-aware.

Open:
- [#144 — Linux-style signal subsystem](https://github.com/Arawn-Davies/Makar/issues/144)
  (slice 8)
- [#145 — Preemption hardening](https://github.com/Arawn-Davies/Makar/issues/145)
  (slice 9)

### Filesystem & devices

The on-disk FAT32 layout adopts Medli's canonical structure
(`/Users/`, `/Apps/x86/`, `/System/Data|Logs|Libraries|Modules/`,
`/Temp/` — see [Medli's `Paths.cs`](https://github.com/Arawn-Davies/Medli/blob/main/Medli/Common/Paths.cs)
and [Makar × Medli](makar-medli.md)).  Alongside it, the kernel
exposes Unix-style synthetic mounts so userspace tools have a uniform
handle on drivers and devices.

Open:
- [#149 — fdisk/cfdisk-style partition tool](https://github.com/Arawn-Davies/Makar/issues/149)
  (interactive create/delete/resize for MBR + GPT; unblocks "install
  Makar onto a blank disk" end-to-end)
- [#150 — `/dev/` synthetic VFS](https://github.com/Arawn-Davies/Makar/issues/150)
  (raw block devices `/dev/sdaN`, character devices `/dev/ttyS0` /
  `/dev/tty0`–`/dev/kbd`, plus `/dev/null|zero|random` — pairs with
  `#149`, since `fdisk /dev/sda` reads/writes through this layer)
- [#151 — Adopt Medli-compatible `/Users/` `/Apps/` `/System/` layout](https://github.com/Arawn-Davies/Makar/issues/151)
  (migrate today's flat `/apps/` to Medli's canonical FAT32 structure
  from [`Paths.cs`](https://github.com/Arawn-Davies/Medli/blob/main/Medli/Common/Paths.cs)
  so user data + binaries move between Makar and Medli without
  translation)
- [#152 — Proper HDD install with GRUB2 (Ubuntu/Arch-style)](https://github.com/Arawn-Davies/Makar/issues/152)
  (today's in-kernel `install` is fragile: whole-disk format, copied
  `core.img`, no `grub.cfg` generation, no module verification.  Fix
  it into a real installer — depends on #149/#150/#151)

Already shipped:
- `/proc/` synthetic filesystem with `cpuinfo`/`meminfo`/`tasks`/`uname`
  (#129)
- FAT32 R/W with auto-mount, ISO 9660 (`/cdrom`)

### Display + terminal

Open:
- [#146 — VGA-text fallback per-TTY backing buffers](https://github.com/Arawn-Davies/Makar/issues/146)
  (slice 16)
- [#148 — UTF-8 terminal with ASCII fallback](https://github.com/Arawn-Davies/Makar/issues/148)
  (slice 13, deferred)

### Shell

Open:
- [#147 — `ps`-style task listing](https://github.com/Arawn-Davies/Makar/issues/147)
  (slice 11)

(`serial shell over COM1` lives under the Makar×Medli theme as [#139](https://github.com/Arawn-Davies/Makar/issues/139).)

## What's already shipped

See [history](history.md) for the changelog.  Highlights:

- Kernel: paging, heap, scheduler (preemptive 100 Hz), per-task page
  directories, ring-3 lifecycle proven by ktest.
- Storage: FAT32 R/W, ISO 9660, IDE PIO, VFS with auto-mount, synthetic
  `/proc`.
- Userspace: ELF loader, ring-3 `iret`, syscall surface (Linux i386
  subset + Makar extensions 200–215), `makbox` busybox-style
  multicall, apps (`calc`, `vix`, `kbtester`, …).
- Display: VESA framebuffer (720p default), VGA 80×50 fallback, per-TTY
  backing grids, tmux-style status bar, four independent TTYs with
  Alt+F1–F4.
- Tooling: single-entrypoint `run.sh`, ccache toolchain image,
  build-once fan-out CI (4 parallel jobs including a UI-test job).

## Tracking conventions

- **Slices** are kernel/userspace work units numbered in CLAUDE.md.
  Each slice should land as one focused PR.
- **Issues** use `feat(<scope>): <topic>` titles for consistency with
  commit messages.  Scopes seen so far: `kernel`, `shell`, `display`,
  `networking`, `makbox`.
- **PRs** reference the issue they close in the body
  (`Closes #N` or `Fixes #N`).  History lives in `docs/history.md`,
  not in PR descriptions — once the PR is merged, the changelog entry
  is the canonical record.
