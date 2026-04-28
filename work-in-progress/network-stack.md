# Network stack — IP/TCP

> **Status:** placeholder — implementation not yet started
> **Milestone:** long-term

## Summary

Implement a minimal network stack (IP/TCP) over a NE2000-compatible NIC
or virtio-net (for QEMU), enabling the same HTTP/FTP/SSH/Telnet daemon
model as Medli.

## Acceptance criteria

- [ ] NE2000 or virtio-net driver (packet send/receive)
- [ ] ARP and IPv4 layer
- [ ] TCP with basic connection management
- [ ] At least one application-layer daemon (e.g. Telnet or FTP)
- [ ] Interoperable with Medli's network daemon model

## References

- Roadmap: [Makar × Medli — long-term](docs/makar-medli.md#long-term)
