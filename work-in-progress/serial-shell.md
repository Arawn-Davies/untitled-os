# Serial shell — UART interactive shell

> **Status:** placeholder — implementation not yet started
> **Milestone:** medium-term

## Summary

Run the same shell command loop over the UART (COM1) so Makar can be
operated headlessly and interoperate with Medli's serial daemon model.

## Acceptance criteria

- [ ] Shell reads from and writes to COM1 as well as the VGA/VESA terminal
- [ ] Both channels active simultaneously (or selectable at compile time)
- [ ] Compatible with Medli-style serial daemon interop
- [ ] Tested via `-serial stdio` in QEMU

## References

- Roadmap: [Makar × Medli — medium-term](docs/makar-medli.md#medium-term)
