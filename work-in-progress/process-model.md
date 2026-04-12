# Process model — round-robin scheduler

> **Status:** placeholder — implementation not yet started
> **Milestone:** medium-term

## Summary

Implement a simple process model: a round-robin scheduler, a fork-like
primitive for spawning processes, and per-process virtual address spaces
built on the existing paging infrastructure.

## Acceptance criteria

- [ ] Process control block (PCB) structure
- [ ] Round-robin scheduler with a configurable time slice
- [ ] `kfork`-like primitive that clones the current address space
- [ ] Per-process page directories using the existing PMM/paging layer
- [ ] Context switch saves and restores all general-purpose registers

## References

- Roadmap: [Makar × Medli — medium-term](docs/makar-medli.md#medium-term)
