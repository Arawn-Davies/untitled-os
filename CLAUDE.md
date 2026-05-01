# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

Makar is a hobby x86 (i386) bare-metal OS kernel written in C and AT&T assembly, booted via GRUB Multiboot 2. It targets 32-bit protected mode and runs in QEMU. Docker wraps the full build/test toolchain — no host cross-compiler is required.

## Build commands

```sh
./docker-qemu.sh         # build interactive ISO + run in QEMU with shell
./docker-ktest.sh        # full CI suite: ktest (TEST_MODE) + GDB boot tests
./generate-hdd.sh        # build installed HDD image (makar-hdd.img, bootable with -boot c)
./generate-hdd.sh --test # build HDD image with TEST_MODE kernel (ktest mode)
./docker-hdd-boot.sh     # clean-build + boot QEMU interactively from the HDD image
./docker-hdd-test.sh     # clean-build + GDB boot test for the HDD image (makar-hdd-test.img)

# Internal (called inside Docker — do not invoke directly):
./docker-iso.sh          # Docker ISO build entry point
./build.sh               # compile kernel + libc (parallel via -j$(nproc))
./iso.sh                 # package into makar.iso
./clean.sh               # remove build artifacts

# Docker Compose equivalents
docker compose run --rm build          # release ISO
docker compose run --rm build-debug    # debug ISO (-O0 -g3)
docker compose run --rm test           # debug build + headless boot test
```

Debug builds use `-O0 -g3`; release uses `-O2 -g`. Override via `CFLAGS`.
Test mode uses `-DTEST_MODE`: kernel runs `ktest_run_all()` then exits QEMU via `isa-debug-exit`.
`build.sh` uses `-j$(nproc)` for parallel compilation automatically.

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
docker run --rm -it -v "$PWD:/work" -w /work arawn780/gcc-cross-i686-elf:fast \
    bash -c 'qemu-system-i386 -cdrom makar.iso -s -S -display none -serial stdio &
             gdb-multiarch src/kernel/makar.kernel -ex "target remote :1234"'
```

**HDD boot test** (installed image, no CD-ROM):
```sh
./generate-hdd.sh        # build makar-hdd.img (grub-mkimage, explicit (hd0,msdos1) prefix)
./generate-hdd.sh --test # same image but kernel compiled with -DTEST_MODE
./docker-hdd-boot.sh     # clean-build + boot interactively from HDD on host QEMU
./docker-hdd-test.sh     # clean-build + GDB test (uses makar-hdd-test.img, not makar-hdd.img)
# outputs: hdd-test-gdb.log, hdd-test-serial.log
```

`generate-hdd.sh` uses the compiler image (`arawn780/gcc-cross-i686-elf:fast`) for the disk-creation step — `dosfstools`, `fdisk`, `grub-pc-bin`, and `grub-common` are all pre-installed. It uses `grub-mkimage` directly (not `grub-install`) to avoid the UUID-search failure that `grub-install` produces when probing loop devices inside Docker. The FAT32 partition receives the kernel at `/boot/makar.kernel` and the userspace binaries from `isodir/apps/` at `/apps/`, making the HDD image self-contained (no CD-ROM required).

`docker-hdd-test.sh` always generates a fresh `makar-hdd-test.img` (separate from the interactive `makar-hdd.img`) so test and interactive images never share state. The GDB test continues to `keyboard_getchar` before checking `fat32_mounted()` to guarantee `vfs_auto_mount()` has completed.

**In-kernel test suite (interactive)**: shell command `ktest` runs all suites from the kernel shell.
At boot (non-TEST_MODE), `ktest_bg_task` runs all suites silently in the background — only prints to VGA on failure; always writes `KTEST_BG: PASS/FAIL` to serial.

**TODO:** HDD test/interactive should use the same kernel binary — mode controlled by a GRUB kernel argument rather than a separate `-DTEST_MODE` build.
- `kernel.c` needs to parse `MULTIBOOT2_TAG_TYPE_CMDLINE` (type 1) and set a runtime `test_mode` flag, replacing `#ifdef TEST_MODE` guards with `if (test_mode)`.
- `multiboot.h` needs `#define MULTIBOOT2_TAG_TYPE_CMDLINE 1`.

## Architecture

### Boot sequence (`kernel_main`)
1. `terminal_initialize` → `init_serial(COM1)` → `init_descriptor_tables` (GDT+IDT)
2. Exception handlers, PMM, paging (256 MiB identity map, 4 MiB pages), heap
3. VESA init + display mode selection: 720p if Bochs VBE available, else 80×50 VGA text
4. Timer (50 Hz PIT), keyboard, IDE/VFS
5. `tasking_init` + `task_create("shell", shell_run)` + `task_create("ktest", ktest_bg_task)`
6. `syscall_init` (registers `int 0x80` handler at IDT gate DPL=3)
7. Idle loop: `task_yield` + `hlt`

### Display mode selection
At boot, `kernel_main` calls `bochs_vbe_available()`. If the Bochs VBE I/O ports respond (QEMU `-vga std`), it sets 1280×720×32 and initialises the VESA TTY at `font_scale=2` (40-col equivalent at this res). If VBE is absent (hardware or minimal QEMU config), it falls back to VGA 80×50 text mode.

The `setmode` shell command can switch freely between any supported resolution at runtime.

### Memory map
- `0x00000000–0x0FFFFFFF` (256 MiB): kernel identity window (4 MiB large pages)
- `0x40000000` (`USER_CODE_BASE`): ring-3 code page
- `0xBFFF0000` (`USER_STACK_TOP`): ring-3 stack top (one 4 KiB page below)

### Tasking
Cooperative round-robin scheduler — no preemption, timer tick only advances accounting. `task_yield()` → context switch via `task_asm.S`. `task_exit()` marks the task DEAD and yields. Pool is fixed-size (`TASK_MAX_TASKS`).

### Syscall ABI (`int 0x80`, Linux i386 convention)
| EAX | Syscall   | Args |
|-----|-----------|------|
| 1   | SYS_EXIT  | — |
| 3   | SYS_READ  | EBX = fd (0=stdin), ECX = buf ptr, EDX = count |
| 4   | SYS_WRITE | EBX = NUL-terminated string ptr (or fd 1) |
| 100 | SYS_DEBUG | EBX = uint32 checkpoint value (prints to VGA + serial) |
| 158 | SYS_YIELD | — |

### Keyboard (Phase 2)
- Arrow-key sentinels at `0x80`–`0x83` (no longer collide with Ctrl codes).
- Per-task 64-byte ring buffers (up to `KB_TASK_SLOTS=4` tasks).
- `keyboard_getchar()` registers the calling task and dequeues from its slot.
- `keyboard_poll()` non-blocking version; falls back to global ring if no slot.
- Ctrl+A prefix arms the pane-switch dispatcher (Ctrl-A,U / Ctrl-A,J).
- Ctrl+C: sets `g_sigint=1` AND routes `\x03` to the focused task's queue.
- `keyboard_sigint_consume()` atomically reads and clears `g_sigint`.

### Shell features
- Inline editing (cursor movement, insert at point).
- History navigation (↑/↓ arrows), up to 16 entries.
- Ctrl+C: abort current input line (prints `^C`, returns empty line to REPL).
- Tab completion: first token completes command names; subsequent tokens complete VFS paths via `vfs_complete()` → `fat32_complete()`.
- `exec <path>`: loads and runs an ELF binary from the VFS. Ctrl+C during exec force-kills the child task.

### VMM (per-task page directories)
- `vmm_create_pd()` — allocates a page directory and mirrors kernel PDEs (indices 0–63)
- `vmm_map_page(pd, vaddr, paddr, flags)` — installs 4 KiB mapping; creates page tables on demand with `PAGE_USER`
- `vmm_switch(pd)` — loads CR3

### Ring-3 entry (`ring3.S`)
`ring3_enter(entry, stack_top)` loads user data selector (0x23) into DS/ES/FS/GS, builds a 5-word `iret` frame (SS=0x23, ESP, EFLAGS|IF, CS=0x1B, EIP), and executes `iret`. Never returns. Caller must call `tss_set_kernel_stack()` and `vmm_switch(pd)` first.

### Userspace apps (`src/userspace/`)
Freestanding ELF binaries built with the cross-compiler. Link against `crt0.S` + `link.ld`. Loaded and executed by `elf_exec()` (shell `exec` command). Available apps:

| Binary | Description |
|--------|-------------|
| `hello.elf` | Hello-world smoke test |
| `calc.elf` | bc-style expression calculator — `+`, `-`, `*`, `/`, `%`, parentheses, recursive-descent parser |

### ktest harness
`KTEST_ASSERT(expr)` — records pass/fail to VGA + serial.
`KTEST_ASSERT_EQ(a, b)` — equality variant.
`KTEST_ASSERT_MAJOR(expr)` — like `KTEST_ASSERT` but calls `kpanic_at` on failure; use for invariants whose violation indicates kernel corruption (GDT validity, PMM sanity, etc.).

## Debug output
- VGA: `t_writestring`, `t_hex`, `t_dec`, `t_putchar` (`include/kernel/tty.h`)
- Serial: `Serial_WriteString`, `Serial_WriteHex` (`include/kernel/serial.h`)
- `KLOG` / `KLOG_HEX` macros — serial only, require `-DDEV_BUILD` compile flag (no-ops in release)
- `SYS_DEBUG` writes to **both** VGA and serial unconditionally — preferred for ring-3 debugging
- `kpanic(msg)` / `KPANIC(msg)` / `kpanic_at(msg, file, func, line)` — renders a panic screen and halts

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
  shell/      shell.c, shell_cmd_{display,disk,fs,apps,system}.c, shell_help.c
  debug/      exception handlers (INT 1/3/8/13/14, serial-first output)
src/kernel/kernel/kernel.c   kernel_main
src/kernel/include/kernel/   all public headers
src/libc/                    minimal freestanding libc (string, stdio, stdlib) → libk.a
src/userspace/               freestanding ELF apps (calc.elf, hello.elf)
tests/                       GDB boot-test suite (gdb_boot_test.py) + test groups
```

## Current state (as of May 2026)

Makar boots to an interactive VESA shell with a working userspace. The major subsystems in place:

- **Display**: VESA framebuffer (Bochs VBE, defaults to 720p), VGA text fallback (80×50). Split-pane abstraction (`vesa_pane_t`) landed; keyboard dispatcher (Phase 2) and VICS pane integration (Phase 3) are pending.
- **Storage**: FAT32 (HDD/USB) + ISO 9660 (CD-ROM) via IDE PIO. VFS layer with CWD, auto-mount.
- **Userspace**: Ring-3 protected mode via `iret`. ELF loader (`elf_exec`). Syscalls: exit, read, write, debug, yield. Apps: `calc.elf`, `hello.elf`.
- **Shell**: Inline editing, history, tab completion (commands + VFS paths), Ctrl+C sigint (aborts line / kills exec'd task).
- **Tasking**: Cooperative round-robin. Background ktest at boot.
- **GRUB**: Two-entry menu (Makar OS + Next available device), 5-second timeout. Both ISO and HDD images.

## Active branches

### `misc/grub-imprv` (current)
GRUB two-entry menu + 5s timeout, 720p default display, silent background ktest, VGA colour-fix on mode switch, Ctrl+C sigint wiring, tab completion, calc app import.

### `feat/split-panes` (Phase 2/3 pending)
Split-pane keyboard dispatcher and VICS pane integration. Phase 1 (pane abstraction) already landed on main.

## Future roadmap

### Near-term kernel work
- **Runtime test_mode via cmdline**: parse `MULTIBOOT2_TAG_TYPE_CMDLINE` in `kernel.c` so ISO and HDD can share a single kernel binary; replace `#ifdef TEST_MODE` guards with a runtime flag.
- **Split-panes Phase 2/3**: Ctrl-A,U/J pane focus switching; refactor VICS to accept a `(top_row, rows)` pane descriptor.
- **Preemptive scheduling**: Add a timer-driven task preemption path. Currently all scheduling is cooperative (`task_yield()`).
- **Signals**: Full `kill()`/`signal()` ABI beyond the current Ctrl+C `g_sigint` flag.

### Userspace / libc porting

The long-term goal is a self-hosting userspace. Prerequisites and approach:

1. **musl libc** (preferred over glibc or uClibc for size):
   - Needs: `mmap`/`munmap`, `brk`/`sbrk`, `read`/`write`/`open`/`close`/`stat`, `fork`/`exec`/`wait` (or at minimum `posix_spawn`), `getpid`, signals.
   - Current blocker: no `fork` — Makar has cooperative tasks, not POSIX processes. Either implement `fork` (requires COW page tables) or target a no-fork musl config (`musl` + `MUSL_NO_FORK` equivalent).
   - Recommended path: add `SYS_OPEN`, `SYS_CLOSE`, `SYS_READ` (file), `SYS_WRITE` (file), `SYS_STAT`, `SYS_LSEEK` first, then `SYS_BRK` (heap extension), then `SYS_MMAP` (anonymous), then attempt musl.

2. **uClibc-ng** (lighter than musl, targets embedded — no fork required for static linking):
   - Still needs the file-I/O syscall set above plus `SYS_GETPID`, `SYS_UNAME`.
   - Static-link userspace apps against uClibc-ng for a known-good libc without porting musl's threading.

3. **bash / dash**:
   - Requires a working libc (musl or uClibc), `fork`+`exec`, file descriptors (stdin/stdout/stderr as VFS fds), `tcgetattr`/`tcsetattr` (terminal), `getenv`/`setenv`, `opendir`/`readdir`.
   - **dash** (POSIX sh, ~150 KiB) is more tractable than bash (~1 MiB) as a first shell port.
   - Near-term stand-in: extend the existing Makar shell with more builtins (pipes, redirection, variables) rather than porting dash immediately.

4. **File-descriptor layer**:
   - Current `SYS_READ` only reads from the keyboard (fd 0). Needs a proper fd table mapping integers to VFS nodes/keyboard/serial.
   - Once fds exist, pipes (`SYS_PIPE`) become straightforward.

5. **Process model**:
   - True POSIX processes require `fork` (COW) + separate address spaces. The current VMM can map per-task page directories; `fork` would clone one.
   - Alternative: implement `posix_spawn` semantics (create + exec without fork) — sufficient for a non-interactive shell and simpler to implement.

### Hardware / platform
- **USB HID keyboard**: currently PS/2 only. QEMU emulates PS/2 by default; real hardware may need USB HID via OHCI/EHCI.
- **Network stack**: would need an e1000 or RTL8139 driver (both well-documented for QEMU), then lwIP or a minimal TCP/IP stack.
- **64-bit (x86-64)**: significant rewrite — new GDT/IDT, long mode entry, 64-bit paging. Worth considering once userspace is stable on i386.
