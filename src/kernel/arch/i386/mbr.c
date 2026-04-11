/*
 * mbr.c — Master Boot Record (MBR) parsing and writing.
 *
 * Reads/writes sector 0 of an ATA drive via ata_read() / ata_write() and
 * interprets the classical four-entry primary partition table.
 */

#include <kernel/mbr.h>
#include <kernel/ata.h>
#include <kernel/tty.h>
#include <kernel/serial.h>
#include <string.h>

/* Byte offsets within the 512-byte MBR sector. */
#define MBR_DISK_SIG_OFFSET   440u
#define MBR_PART_TABLE_OFFSET 446u
#define MBR_BOOT_SIG_OFFSET   510u

int mbr_read(int drive_idx, mbr_t *out)
{
    uint8_t sector[ATA_SECTOR_SIZE];

    if (ata_read(drive_idx, 0, 1, sector) < 0)
        return -1;

    /* Check the 0xAA55 boot signature (stored little-endian). */
    uint16_t sig = (uint16_t)sector[MBR_BOOT_SIG_OFFSET] |
                   ((uint16_t)sector[MBR_BOOT_SIG_OFFSET + 1] << 8);
    out->valid = (sig == MBR_BOOT_SIGNATURE);

    /* Disk signature (4 bytes at offset 440). */
    memcpy(&out->disk_sig, sector + MBR_DISK_SIG_OFFSET, sizeof(uint32_t));

    /* Copy all four 16-byte partition entries. */
    for (int i = 0; i < MBR_MAX_PARTITIONS; i++) {
        memcpy(&out->parts[i],
               sector + MBR_PART_TABLE_OFFSET + i * 16,
               sizeof(mbr_entry_t));
    }

    return out->valid ? 0 : -2;
}

int mbr_write(int drive_idx, const mbr_t *mbr)
{
    /* Read the current sector first so we preserve the bootstrap code. */
    uint8_t sector[ATA_SECTOR_SIZE];
    if (ata_read(drive_idx, 0, 1, sector) < 0)
        return -1;

    /* Overwrite the disk signature. */
    memcpy(sector + MBR_DISK_SIG_OFFSET, &mbr->disk_sig, sizeof(uint32_t));

    /* Overwrite the partition table. */
    for (int i = 0; i < MBR_MAX_PARTITIONS; i++) {
        memcpy(sector + MBR_PART_TABLE_OFFSET + i * 16,
               &mbr->parts[i],
               sizeof(mbr_entry_t));
    }

    /* Ensure the boot signature is present. */
    sector[MBR_BOOT_SIG_OFFSET]     = 0x55;
    sector[MBR_BOOT_SIG_OFFSET + 1] = 0xAA;

    return ata_write(drive_idx, 0, 1, sector);
}

/* ---------------------------------------------------------------------------
 * mbr_print helpers
 * --------------------------------------------------------------------------- */

/*
 * Known partition type IDs — enough to be useful without bloating the kernel.
 * Anything not listed is shown as a raw hex byte.
 */
static const char *part_type_name(uint8_t type)
{
    switch (type) {
    case 0x00: return "Empty";
    case 0x01: return "FAT12";
    case 0x04: return "FAT16 <32M";
    case 0x05: return "Extended";
    case 0x06: return "FAT16";
    case 0x07: return "NTFS/exFAT";
    case 0x0B: return "FAT32 CHS";
    case 0x0C: return "FAT32 LBA";
    case 0x0E: return "FAT16 LBA";
    case 0x0F: return "Extended LBA";
    case 0x82: return "Linux swap";
    case 0x83: return "Linux ext";
    case 0xFA: return "MDFS";     /* Makar/Medli shared FS — see MBR_TYPE_MDFS */
    case 0xFB: return "MDFS-alt";
    default:   return NULL;
    }
}

void mbr_print(int drive_idx, const mbr_t *mbr)
{
    const ata_drive_t *drv = ata_get_drive(drive_idx);
    if (!drv || !drv->present) {
        t_writestring("disk: no drive at index ");
        t_dec((uint32_t)drive_idx);
        t_putchar('\n');
        return;
    }

    t_writestring("Drive ");
    t_dec((uint32_t)drive_idx);
    t_writestring(" (");
    t_writestring(drv->model[0] ? drv->model : "unknown");
    t_writestring(")\n");

    t_writestring("  Serial : ");
    t_writestring(drv->serial[0] ? drv->serial : "n/a");
    t_putchar('\n');

    t_writestring("  Size   : ");
    t_dec(drv->sectors / 2048);
    t_writestring(" MiB (");
    t_dec(drv->sectors);
    t_writestring(" sectors)\n");

    if (!mbr->valid) {
        t_writestring("  No valid MBR (bad boot signature)\n");
        return;
    }

    t_writestring("  Disk sig: 0x");
    t_hex(mbr->disk_sig);
    t_putchar('\n');
    t_writestring("  Partitions:\n");

    int any = 0;
    for (int i = 0; i < MBR_MAX_PARTITIONS; i++) {
        const mbr_entry_t *p = &mbr->parts[i];
        if (p->type == 0)
            continue;
        any = 1;

        t_writestring("    [");
        t_dec((uint32_t)(i + 1));
        t_writestring("] type=0x");
        t_hex((uint32_t)p->type);

        const char *name = part_type_name(p->type);
        if (name) {
            t_writestring(" (");
            t_writestring(name);
            t_putchar(')');
        }

        t_writestring("  start=");
        t_dec(p->lba_start);
        t_writestring("  sectors=");
        t_dec(p->sector_count);
        t_writestring("  size=");
        t_dec(p->sector_count / 2048);
        t_writestring(" MiB");

        if (p->status & 0x80)
            t_writestring("  [boot]");

        t_putchar('\n');
    }

    if (!any)
        t_writestring("    (no partitions)\n");
}
