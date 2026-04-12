# userspace (Ring 3)

This directory contains all Ring-3 (userspace) utility programs for Makar.
Each utility is built as a separate freestanding i686 ELF32 binary and
installed to `/boot/apps/` on the ISO.

## How it works

Programs are compiled with `i686-elf-gcc -ffreestanding -nostdlib` and linked
against a tiny `crt0.S` that:

1. Reads `argc` and `argv` from the ELF process stack (placed there by the
   kernel ELF loader).
2. Calls `main()`.
3. Issues `int 0x80` with `EAX=SYS_EXIT` to terminate the task.

The only kernel interface available is `int 0x80` (see `include/sys.h`).
Syscall numbers mirror `src/kernel/include/kernel/syscall.h`.

## Current utilities

| Binary   | Description                     |
|----------|---------------------------------|
| `echo`   | Print arguments to the terminal |
| `true`   | Exit successfully (code 0)      |
| `false`  | Exit unsuccessfully (code 1)    |

## Directory layout

```
src/userspace/
  Makefile          ← builds all utilities and installs to /boot/apps/
  include/
    sys.h           ← int 0x80 syscall wrappers
  arch/
    i386/
      crt0.S        ← ELF _start: reads argc/argv, calls main(), sys_exit()
      linker.ld     ← user-space ELF linker script (base 0x08048000)
  echo/
    echo.c
  true/
    true.c
  false/
    false.c
```

## Adding a new utility

1. Create a subdirectory `src/userspace/<name>/`.
2. Write `<name>.c` with a `main(int argc, char **argv)` function.
   Use `sys_write()` from `<sys.h>` for terminal output.
3. Add a compilation rule and a link rule to `Makefile` (follow the `echo`
   pattern).
4. Add the binary name to the `BINS` list in `Makefile`.
5. Add a dependency line to the `clean` and `-include` entries.

## Notes

- These binaries are on the ISO now and will be loadable once the kernel's
  ELF loader (`cmd_run`) and process model are complete.
- The IDT gate for `int 0x80` currently runs at DPL=0 (kernel privilege).
  Opening it to DPL=3 for true user-mode execution requires a TSS and is
  part of the upcoming process model work.

