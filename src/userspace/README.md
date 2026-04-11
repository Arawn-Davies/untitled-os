# userspace (Ring 3)

This directory is the future home of all Ring-3 (userspace) programs and
libraries for Makar.

At the moment the OS runs entirely in Ring 0 (kernelspace).  Once a proper
syscall interface, user-mode memory layout, and ELF loader are in place, user
programs will live here.

Planned structure (subject to change):

```
src/userspace/
  init/        # PID-1 init process
  shell/       # interactive user shell
  lib/         # userspace-only libraries (linked against libc, not libk)
```

See `src/kernel/` for the kernelspace (Ring 0) implementation and
`src/libc/` for the C library that will be shared between both rings.
