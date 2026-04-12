# Shell — minimal interactive command loop

> **Status:** placeholder — implementation not yet started
> **Milestone:** near-term

## Summary

Implement a minimal interactive shell that reads keyboard input and
executes built-in commands, matching the Medli command vocabulary.

## Acceptance criteria

- [ ] Read line from keyboard driver ring buffer with echo and backspace
- [ ] Dispatch built-in commands: `help`, `clear`, `ls`, `cd`, `echo`
- [ ] Welcome message and ASCII logo on boot
- [ ] Kernel version string displayed at startup
- [ ] Consistent prompt matching Medli UX

## References

- Roadmap: [Makar × Medli — near-term](docs/makar-medli.md#near-term)
- UX conventions: [Makar × Medli — shared UX conventions](docs/makar-medli.md#shared-ux-conventions)
