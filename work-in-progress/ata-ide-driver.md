# ATA/IDE driver — PIO-mode disk access

> **Status:** placeholder — implementation not yet started
> **Milestone:** near-term

## Summary

Implement a PIO-mode ATA/IDE disk driver to provide block-level
read/write access, enabling the FAT32 driver to persist data.

## Acceptance criteria

- [ ] Detect primary ATA bus master and slave drives
- [ ] Implement 28-bit LBA PIO read (512-byte sectors)
- [ ] Implement 28-bit LBA PIO write
- [ ] Tested under QEMU with a disk image (`-hda`)

## References

- Roadmap: [Makar × Medli — near-term](docs/makar-medli.md#near-term)
- OSDev wiki: https://wiki.osdev.org/ATA_PIO_Mode
