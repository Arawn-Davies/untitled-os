# descr_tbl — GDT and IDT initialisation

**Header:** `kernel/include/kernel/descr_tbl.h`  
**Source:** `kernel/arch/i386/descr_tbl.c`  
**ASM stubs:** `kernel/arch/i386/dt_asm.S`

Sets up the two fundamental x86 descriptor tables required before the CPU can
run protected-mode code correctly: the Global Descriptor Table (GDT) and the
Interrupt Descriptor Table (IDT).

---

## Data structures

### `gdt_entry_t`

```c
struct gdt_entry_struct {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed));
```

One entry in the GDT.  Each entry describes a memory segment: its base
address, limit, privilege level, and flags.  The `packed` attribute prevents
the compiler from inserting alignment padding.

### `gdt_ptr_t`

```c
struct gdt_ptr_struct {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));
```

The 6-byte pseudo-descriptor passed to the `LGDT` instruction.  `limit` is
`(sizeof(gdt_entry_t) * count) - 1`; `base` is the linear address of the
first GDT entry.

### `idt_entry_t`

```c
struct idt_entry_struct {
    uint16_t base_lo;
    uint16_t sel;
    uint8_t  always0;
    uint8_t  flags;
    uint16_t base_hi;
} __attribute__((packed));
```

One 8-byte interrupt gate descriptor.  `base_lo`/`base_hi` together form the
32-bit address of the ISR stub; `sel` is the kernel code segment selector
(`0x08`); `flags` encodes the gate type and privilege level.

### `idt_ptr_t`

```c
struct idt_ptr_struct {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));
```

The 6-byte pseudo-descriptor passed to the `LIDT` instruction.

---

## GDT layout

Five segments are installed at boot:

| Index | Selector | Type | Description |
|---|---|---|---|
| 0 | `0x00` | Null | Required null descriptor. |
| 1 | `0x08` | Code | Kernel code segment (ring 0, 0–4 GiB). |
| 2 | `0x10` | Data | Kernel data segment (ring 0, 0–4 GiB). |
| 3 | `0x18` | Code | User mode code segment (ring 3, 0–4 GiB). |
| 4 | `0x20` | Data | User mode data segment (ring 3, 0–4 GiB). |

The user-mode segments are provisioned for future use; Makar currently runs
entirely in ring 0.

---

## IDT layout

256 gates are installed.  Vectors 0–31 map to the CPU exception stubs
(`isr0`–`isr31`); vectors 32–47 map to the hardware IRQ stubs
(`irq0`–`irq15`) after the 8259 PIC has been remapped.

---

## Functions

### `init_descriptor_tables`

```c
void init_descriptor_tables(void);
```

Public entry point called from `kernel_main`.  Initialises the GDT, loads it
with `gdt_flush`, initialises the IDT (including remapping the 8259 PIC),
loads it with `idt_flush`, then registers the ISR dispatch stubs via
`init_isr_handlers()`.
