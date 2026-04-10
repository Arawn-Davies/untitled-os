# FAT32 driver — read/write access

> **Status:** placeholder — implementation not yet started
> **Milestone:** near-term

## Summary

Implement a FAT32 filesystem driver providing read and write access to
FAT32 volumes, adopting the shared Makar/Medli directory layout.

## Acceptance criteria

- [ ] Mount a FAT32 volume (backed by the ATA/IDE driver)
- [ ] Read files and directories using the `0:\` volume root convention
- [ ] Write files and create directories
- [ ] Adopt the shared layout: `Users\`, `Apps\`, `System\`, `Temp\`
- [ ] Path separator is `\` (matching Medli `Paths.Separator`)

## References

- Roadmap: [Makar × Medli — near-term](docs/makar-medli.md#near-term)
- Filesystem layout: [Makar × Medli — filesystem layout](docs/makar-medli.md#filesystem-layout)
