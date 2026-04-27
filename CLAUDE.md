# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

Makar is a hobby x86 (i386) bare-metal OS kernel written in C and AT&T assembly, booted via GRUB Multiboot 2. It targets 32-bit protected mode and runs in QEMU. Docker wraps the full build/test toolchain — no host cross-compiler is required.

## Build commands

```sh
./docker-qemu.sh         # build interactive ISO + run in QEMU with shell
./docker-ktest.sh        # full CI suite: ktest (TEST_MODE) + GDB boot tests

# Internal (called inside Docker — do not invoke directly):
./docker-iso.sh          # Docker ISO build entry point
./build.sh               # compile kernel + libc
./iso.sh                 # package into makar.iso
./clean.sh               # remove build artifacts

# Docker Compose equivalents
docker compose run --rm build          # release ISO
docker compose run --rm build-debug    # debug ISO (-O0 -g3)
docker compose run --rm test           # debug build + headless boot test
```

Debug builds use `-O0 -g3`; release uses `-O2 -g`. Override via `CFLAGS`.
Test mode uses `-DTEST_MODE`: kernel runs `ktest_run_all()` then exits QEMU via `isa-debug-exit`.

## Testing

**Full CI suite** (ktest + GDB boot checkpoints, auto-exits):
```sh
./docker-ktest.sh
# Step 1: TEST_MODE ISO → ktest_run_all() → QEMU exits.  Output: ktest.log
# Step 2: debug ISO → GDB boot-checkpoint verification.  Output: gdb-test.log
# exits 0 on pass, 1 on any failure
```

GDB test script: `tests/gdb_boot_test.py`. Connects to `:1234`, verifies Multiboot 2 magic, symbol addresses, memory layout, boot checkpoints. Output: `gdb-test.log`.

**Interactive GDB debug** (inside Docker container manually):
```sh
# Run QEMU with GDB stub from inside the Docker container
docker run --rm -it -v "$PWD:/work" -w /work arawn780/gcc-cross-i686-elf:fast \
    bash -c 'qemu-system-i386 -cdrom makar.iso -s -S -display none -serial stdio &
             gdb-multiarch src/kernel/makar.kernel -ex "target remote :1234"'
```

**In-kernel test suite (interactive)**: shell command `ktest` runs all suites from the kernel shell.

## Architecture

### Boot sequence (`kernel_main`)
1. `terminal_initialize` → `init_serial(COM1)` → `init_descriptor_tables` (GDT+IDT)
2. Exception handlers, PMM, paging (256 MiB identity map, 4 MiB pages), heap
3. VESA, timer (50 Hz PIT), keyboard, IDE/VFS
4. `tasking_init` + `task_create("shell", shell_run)`
5. `syscall_init` (registers `int 0x80` handler at IDT gate DPL=3)
6. Idle loop: `task_yield` + `hlt`

### Memory map
- `0x00000000–0x0FFFFFFF` (256 MiB): kernel identity window (4 MiB large pages)
- `0x40000000` (`USER_CODE_BASE`): ring-3 code page
- `0xBFFF0000` (`USER_STACK_TOP`): ring-3 stack top (one 4 KiB page below)

### Tasking
Cooperative round-robin scheduler — no preemption, timer tick only advances accounting. `task_yield()` → context switch via `task_asm.S`. `task_exit()` marks the task DEAD and yields. Pool is fixed-size.

### Syscall ABI (`int 0x80`, Linux i386 convention)
| EAX | Syscall   | Args |
|-----|-----------|------|
| 1   | SYS_EXIT  | — |
| 4   | SYS_WRITE | EBX = NUL-terminated string ptr |
| 100 | SYS_DEBUG | EBX = uint32 checkpoint value (prints to VGA + serial) |
| 158 | SYS_YIELD | — |

### VMM (per-task page directories)
- `vmm_create_pd()` — allocates a page directory and mirrors kernel PDEs (indices 0–63)
- `vmm_map_page(pd, vaddr, paddr, flags)` — installs 4 KiB mapping; creates page tables on demand with `PAGE_USER`
- `vmm_switch(pd)` — loads CR3

### Ring-3 entry (`ring3.S`)
`ring3_enter(entry, stack_top)` loads user data selector (0x23) into DS/ES/FS/GS, builds a 5-word `iret` frame (SS=0x23, ESP, EFLAGS|IF, CS=0x1B, EIP), and executes `iret`. Never returns. Caller must call `tss_set_kernel_stack()` and `vmm_switch(pd)` first.

### Ring-3 smoke test (`ring3test` shell command)
Shell command `ring3test` → `task_create("ring3test", usertest_task)` → `task_yield`.

`usertest_task` sets up a VMM page directory, maps code (USER_CODE_BASE, user-RO) and stack (USER_STACK_TOP-4K, user-RW) pages, activates the PD, sets the TSS kernel stack, then calls `ring3_enter`.

The embedded PIC binary (`user_test_bin` in `usertest.c`) executes:
`SYS_DEBUG(1)` → `SYS_WRITE("Welcome to userspace!\n")` → `SYS_DEBUG(2)` → `SYS_EXIT(0)`.

To diagnose failures: watch `serial.log` for `[ring3] CP: 0x1` and `[ring3] CP: 0x2`. CP1 appears before any syscall; CP2 appears after the write but before exit. If CP1 appears but not CP2, the write syscall is the fault; if neither appears, ring-3 entry itself failed.

## Debug output
- VGA: `t_writestring`, `t_hex`, `t_dec`, `t_putchar` (`include/kernel/tty.h`)
- Serial: `Serial_WriteString`, `Serial_WriteHex` (`include/kernel/serial.h`)
- `KLOG` / `KLOG_HEX` macros — serial only, require `-DDEV_BUILD` compile flag (no-ops in release)
- `SYS_DEBUG` writes to **both** VGA and serial unconditionally — preferred for ring-3 debugging

## Key source layout
```
src/kernel/arch/i386/
  boot/       boot.S (Multiboot 2 entry), crti/crtn
  core/       GDT/IDT (descr_tbl.c), ISR stub (isr_asm.S), interrupt dispatch (isr.c)
  mm/         pmm.c (frame allocator), paging.c, vmm.c (per-task page dirs), heap.c
  drivers/    serial, keyboard, timer, IDE, ACPI, partition
  fs/         fat32.c, iso9660.c, vfs.c
  display/    tty.c (VGA text), vesa.c + vesa_tty.c (VESA framebuffer)
  proc/       task.c + task_asm.S (scheduler), syscall.c, ring3.S, usertest.c, ktest.c
  shell/      shell.c, shell_cmds.c, shell_help.c
  debug/      exception handlers (INT 1/3/8/13/14, serial-first output)
src/kernel/kernel/kernel.c   kernel_main
src/kernel/include/kernel/   all public headers
src/libc/                    minimal freestanding libc (string, stdio, stdlib) → libk.a
tests/                       GDB boot-test suite (gdb_boot_test.py) + test groups
```

## Active branch: `feat/split-panes`
Current focus: tmux-style split panes in the VESA renderer (with VGA / low-res fallback to virtual consoles).  The work is staged in three phases:

1. **Phase 1 (✅ implemented, awaiting commit/PR)** — pane abstraction in `vesa_tty`.  `vesa_pane_t` owns its cursor, colours, and a top_row/rows sub-rectangle of the screen.  Legacy `vesa_tty_*` calls delegate to a screen-spanning `default_pane`, so existing callers are unaffected.  Full ktest + GDB boot suite passes.
2. **Phase 2** — keyboard dispatcher with a Ctrl-A prefix.  In split-mode (VESA, ≥ ~30 rows) Ctrl-A,U / Ctrl-A,J switch focus between the top and bottom panes.  In VGA text mode or low-res VESA, the same prefix selects a full-screen virtual console (Ctrl-A,1/2/3).  Per-task input rings; `keyboard_getchar()` dequeues from the calling task's bound queue.
3. **Phase 3** — refactor `vics.c` to take a pane (top_row, text_rows, status_row) instead of hard-coding rows 0–23 + 24 + raw `VGA_MEMORY` writes.  Add a `splitscreen` shell command that spawns VICS in the top pane and continues the shell in the bottom.

Split mode is VESA-only.  VGA text and low-res VESA (< ~30 rows) fall back to virtual-console switching with the same Ctrl-A prefix.

The previous focus (ring-3 userspace) landed on `main` in PRs #53 and #112.

### Pane API (Phase 1, landed on this branch)

`include/kernel/vesa_tty.h` defines:

```c
typedef struct vesa_pane {
    uint32_t top_row, cols, rows;     /* sub-rect — full width, rows [top_row, top_row+rows) */
    uint32_t cur_col, cur_row;        /* pane-relative cursor */
    uint32_t fg, bg;                  /* framebuffer-pixel form (already composed) */
} vesa_pane_t;
```

Public pane API: `vesa_tty_default_pane()`, `vesa_tty_pane_init(p, top_row, rows)`, `vesa_tty_pane_setcolor(p, fg_rgb, bg_rgb)` (takes 0x00RRGGBB and composes), `vesa_tty_pane_putchar/_put_at/_set_cursor/_clear/_get_col/_get_row`.

Implementation notes (`arch/i386/display/vesa_tty.c`):
- `font_scale = 2` by default → 16×16 cells over an 8×8 glyph.  At 640×480 that's 40 cols × 30 rows.
- `pane_scroll_up(p)` does an in-pane `memmove` over framebuffer scanlines and clears the bottom row to `p->bg`.  Other panes are untouched.
- `vesa_tty_clear()` (legacy) intentionally clears the **whole** screen (`vesa_clear(default_pane.bg)`), not just the default pane region — preserves prior behaviour for any sub-pane carve-up.
- `vesa_tty_spinner_tick` always renders into `default_pane` at the top-right of the physical screen, regardless of focus.
- Stack-allocate `vesa_pane_t` — no heap dependency.

Constraint: side-by-side (column) splits are out of scope.  Only horizontal stacking (top/bottom) is supported.

### Phase 2 / 3 reference points (not yet touched)

- **VGA text mode renderer**: `arch/i386/display/tty.c` writes directly to `0xB8000` (`VGA_MEMORY`).  Stays single-pane; virtual-console switching swaps a backing buffer rather than carving rows.
- **VICS** (visual editor): `vics.c` currently hard-codes 24 text rows + status row 24 and writes raw cells to `VGA_MEMORY`.  Phase 3 refactors it to accept `(top_row, text_rows, status_row)` and route through either VGA or VESA pane writers.
- **Keyboard**: `drivers/keyboard.c` exposes a single `keyboard_getchar()` ring.  Phase 2 introduces per-task input queues + a focus pointer; the Ctrl-A prefix is consumed by the dispatcher and never reaches the focused task.
- **Mode detection for split-eligibility**: check `vesa_tty_is_ready()` and `vesa_tty_get_rows() >= 30`.  Otherwise fall back to virtual consoles.
