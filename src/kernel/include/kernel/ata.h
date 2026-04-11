#ifndef _KERNEL_ATA_H
#define _KERNEL_ATA_H

/*
 * ata.h — ATA/IDE PIO mode driver (primary bus, master + slave).
 *
 * Supports 28-bit LBA reads and writes over the primary ATA bus
 * (I/O base 0x1F0, control 0x3F6) in polling (PIO) mode — no IRQs required.
 *
 * Drive indices:
 *   ATA_DRIVE_MASTER (0) — primary master
 *   ATA_DRIVE_SLAVE  (1) — primary slave
 */

#include <stdint.h>

#define ATA_SECTOR_SIZE   512
#define ATA_MAX_DRIVES    2

#define ATA_DRIVE_MASTER  0
#define ATA_DRIVE_SLAVE   1

/* Information populated by ata_init() for each detected drive. */
typedef struct {
    int      present;   /* 1 if the drive was detected and is ATA (not ATAPI) */
    uint32_t sectors;   /* total addressable sectors (28-bit LBA)              */
    char     model[41]; /* model string from IDENTIFY, NUL-terminated          */
    char     serial[21];/* serial number from IDENTIFY, NUL-terminated         */
} ata_drive_t;

/*
 * ata_init — probe the primary ATA bus and populate the drive table.
 * Must be called after heap_init().  Safe to call if no drives are present.
 */
void ata_init(void);

/*
 * ata_read — read `count` sectors starting at `lba` from drive `drive_idx`
 * into the caller-supplied buffer `buf` (must be ≥ count × ATA_SECTOR_SIZE).
 * Returns 0 on success, -1 on error.
 */
int ata_read(int drive_idx, uint32_t lba, uint8_t count, void *buf);

/*
 * ata_write — write `count` sectors from `buf` to drive `drive_idx` starting
 * at `lba`.  Flushes the write cache afterwards.
 * Returns 0 on success, -1 on error.
 */
int ata_write(int drive_idx, uint32_t lba, uint8_t count, const void *buf);

/*
 * ata_get_drive — return a pointer to the drive-info struct for `idx`, or
 * NULL if `idx` is out of range.
 */
const ata_drive_t *ata_get_drive(int idx);

/* ata_drive_count — return the number of detected drives (0, 1, or 2). */
int ata_drive_count(void);

#endif /* _KERNEL_ATA_H */
