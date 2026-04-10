# Keyboard driver — PS/2 IRQ 1

> **Status:** placeholder — implementation not yet started
> **Milestone:** near-term

## Summary

Implement a PS/2 keyboard driver that handles IRQ 1 and translates
hardware scan codes to ASCII characters for use by the shell.

## Acceptance criteria

- [ ] IRQ 1 handler registered and enabled in the IDT
- [ ] Set 1 scan codes translated to ASCII (printable + control keys)
- [ ] Key events delivered to a ring buffer consumable by the shell
- [ ] Tested in QEMU (`keyboard: PS/2 IRQ1 handler registered` on serial)

## References

- Roadmap: [Makar × Medli — near-term](docs/makar-medli.md#near-term)
- OSDev wiki: https://wiki.osdev.org/PS/2_Keyboard
