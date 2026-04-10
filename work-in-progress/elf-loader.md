# ELF loader — ELF32 static binary execution

> **Status:** placeholder — implementation not yet started
> **Milestone:** medium-term

## Summary

Implement an ELF32 loader that reads static ELF executables from the
FAT32 filesystem and maps them into a new process address space for
execution.

## Acceptance criteria

- [ ] Parse ELF32 header and program headers
- [ ] Map `PT_LOAD` segments into the process address space
- [ ] Jump to entry point in user or kernel context
- [ ] Executables resolved from `Apps\\x86\` on the FAT32 volume

## References

- Roadmap: [Makar × Medli — medium-term](docs/makar-medli.md#medium-term)
- Binary compatibility: [Makar × Medli — binary compatibility](docs/makar-medli.md#binary-compatibility-goals)
