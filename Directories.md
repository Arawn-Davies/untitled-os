# Project Layout

Makar's source tree is split between **kernelspace** (Ring 0) and the
future **userspace** (Ring 3), mirroring the structure used in
[Medli](https://github.com/Arawn-Davies/Medli).

---

## `src/kernel/` — Kernelspace (Ring 0)

The bare-metal kernel, compiled with `-ffreestanding` and linked against
`libk`.  Architecture-specific code lives under `arch/<arch>/` and is
further divided into logical groups:

| Subdirectory | Contents |
|---|---|
| `arch/i386/init/` | Early boot (`boot.S`, C-runtime stubs `crti.S` / `crtn.S`) |
| `arch/i386/core/` | CPU descriptor tables (GDT/IDT), ISR / IRQ dispatch |
| `arch/i386/mm/` | Physical memory manager, paging, kernel heap |
| `arch/i386/hardware/` | Device drivers: serial, timer, keyboard, IDE |
| `arch/i386/display/` | VGA text terminal, VESA linear framebuffer |
| `arch/i386/system/` | Syscall layer, task/scheduler skeleton, kernel shell |
| `arch/i386/debug/` | Debug helpers, GDB-friendly trap handlers |
| `kernel/` | Portable kernel entry point (`kernel.c`) |
| `include/kernel/` | Public kernel headers |

---

## `src/libc/` — C Standard Library

A freestanding C library built in two flavours:

* **`libk`** — kernel variant (`-D__is_libk`), linked into the kernel.
* **`libc`** — hosted variant (not yet built), intended for future userspace.

Provides `stdio`, `stdlib`, and `string` primitives.

---

## `src/userspace/` — Userspace (Ring 3)

*Placeholder — not yet built.*

This is the future home of Ring-3 programs (init, shell, user utilities).
Once the kernel exposes a stable syscall ABI and ELF loader, user programs
will be added here and linked against `libc`.

See [`src/userspace/README.md`](src/userspace/README.md) for the planned
sub-structure.

---

## Other top-level directories

| Path | Purpose |
|---|---|
| `docs/` | Subsystem documentation (one `.md` per module) |
| `tests/` | Automated test scripts and GDB boot tests |
| `LICENSES/` | Third-party licence texts |

---

## Top-level scripts

| Script | Purpose |
|---|---|
| `build.sh` | Build all projects into the sysroot |
| `iso.sh` | Build `makar.iso` via `grub-mkrescue` |
| `qemu.sh` | Build and run in QEMU |
| `docker-iso.sh` | Build `makar.iso` in the CI Docker image |
| `docker-test.sh` | Build in Docker, run host-QEMU serial smoke test |
| `docker-qemu.sh` | Build in Docker, run interactively in host QEMU |
| `gdb.sh` | Build and launch with GDB stub on `:1234` |
| `clean.sh` | Remove build artefacts |
| `mkhdd.sh` | Create a raw hard-disk image |
