#ifndef _KERNEL_MBR_H
#define _KERNEL_MBR_H

/*
 * mbr.h — Master Boot Record (MBR) parsing.
 *
 * Reads sector 0 of an ATA drive via ata_read() and interprets the classical
 * 512-byte MBR layout:
 *
 *   Offset   Length   Field
 *   ------   ------   -----
 *    0        440     Bootstrap code (ignored)
 *   440        4      Optional disk signature
 *   444        2      Reserved (0x0000)
 *   446       64      Four 16-byte primary partition entries
 *   510        2      Boot signature (0x55, 0xAA)
 *
 * Each partition entry:
 *   Offset   Length   Field
 *   ------   ------   -----
 *    0        1       Status (0x80 = bootable, 0x00 = inactive)
 *    1        3       CHS of first sector (legacy, ignored)
 *    4        1       Partition type / filesystem ID
 *    5        3       CHS of last sector (legacy, ignored)
 *    8        4       LBA of first sector (little-endian)
 *   12        4       Number of sectors   (little-endian)
 */

#include <stdint.h>

#define MBR_MAX_PARTITIONS  4
#define MBR_BOOT_SIGNATURE  0xAA55u

/* One entry from the partition table (matches the on-disk layout). */
typedef struct __attribute__((packed)) {
    uint8_t  status;        /* 0x80 = bootable, 0x00 = not                     */
    uint8_t  chs_first[3]; /* CHS address of first sector (legacy, ignored)     */
    uint8_t  type;          /* Partition type / filesystem ID (0 = empty entry)  */
    uint8_t  chs_last[3];  /* CHS address of last sector (legacy, ignored)      */
    uint32_t lba_start;    /* LBA of first sector                               */
    uint32_t sector_count; /* Number of sectors in this partition               */
} mbr_entry_t;

/* Parsed representation of an MBR. */
typedef struct {
    uint32_t    disk_sig;                    /* disk signature at offset 440        */
    mbr_entry_t parts[MBR_MAX_PARTITIONS];  /* up to 4 primary partition entries   */
    int         valid;                       /* 1 if boot signature 0xAA55 matched  */
} mbr_t;

/*
 * mbr_read — read and parse the MBR from ATA drive `drive_idx`.
 *
 * Returns:
 *   0   success (out->valid is set, but there may be no partitions)
 *  -1   ATA read error
 *  -2   boot signature absent (not a valid MBR partition table)
 */
int mbr_read(int drive_idx, mbr_t *out);

/*
 * mbr_print — write a human-readable summary of the drive and its partition
 * table to the VGA terminal.
 */
void mbr_print(int drive_idx, const mbr_t *mbr);

/*
 * mbr_write — write a modified MBR back to sector 0 of drive `drive_idx`.
 * `mbr` must have been obtained from mbr_read() first.
 *
 * Returns 0 on success, -1 on error.
 */
int mbr_write(int drive_idx, const mbr_t *mbr);

#endif /* _KERNEL_MBR_H */
