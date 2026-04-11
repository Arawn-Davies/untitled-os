/*
 * ata.c — ATA/IDE PIO mode driver.
 *
 * Supports the primary ATA bus (0x1F0–0x1F7, control 0x3F6) in polling
 * (PIO) mode.  No IRQs are used; all transfers are completed synchronously
 * by spinning on the status register.
 *
 * References:
 *   https://wiki.osdev.org/ATA_PIO_Mode
 *   https://wiki.osdev.org/ATA/ATAPI_using_DMA  (background only)
 */

#include <kernel/ata.h>
#include <kernel/asm.h>
#include <kernel/tty.h>
#include <kernel/serial.h>
#include <string.h>

/* ---------------------------------------------------------------------------
 * Primary ATA bus I/O ports
 * --------------------------------------------------------------------------- */

#define ATA_BASE  0x1F0u  /* command block registers base */
#define ATA_CTRL  0x3F6u  /* control / alternate-status register */

/* Register offsets from ATA_BASE */
#define ATA_REG_DATA      0u  /* 16-bit data port (R/W)            */
#define ATA_REG_ERROR     1u  /* error information (read)           */
#define ATA_REG_FEATURES  1u  /* features (write)                   */
#define ATA_REG_SECCOUNT  2u  /* sector count                       */
#define ATA_REG_LBA_LO    3u  /* LBA bits  7: 0                     */
#define ATA_REG_LBA_MID   4u  /* LBA bits 15: 8                     */
#define ATA_REG_LBA_HI    5u  /* LBA bits 23:16                     */
#define ATA_REG_DRIVE     6u  /* drive select + LBA bits 27:24      */
#define ATA_REG_STATUS    7u  /* status (read)                      */
#define ATA_REG_COMMAND   7u  /* command (write)                    */

/* Status register bits */
#define ATA_SR_ERR   0x01u  /* error                   */
#define ATA_SR_DRQ   0x08u  /* data request ready      */
#define ATA_SR_DF    0x20u  /* drive fault             */
#define ATA_SR_DRDY  0x40u  /* drive ready             */
#define ATA_SR_BSY   0x80u  /* busy                    */

/*
 * Drive byte for the drive/head register:
 *   bit 7 = 1 (reserved, must be set)
 *   bit 6 = 1 → LBA mode
 *   bit 5 = 1 (reserved, must be set)
 *   bit 4 = drive select (0 = master, 1 = slave)
 *   bits 3:0 = LBA bits 27:24
 */
#define ATA_SEL_MASTER  0xA0u  /* for IDENTIFY (no LBA bits needed) */
#define ATA_SEL_SLAVE   0xB0u
#define ATA_LBA_MASTER  0xE0u  /* for 28-bit LBA reads/writes       */
#define ATA_LBA_SLAVE   0xF0u

/* ATA commands */
#define ATA_CMD_READ     0x20u  /* READ SECTORS (with retry) */
#define ATA_CMD_WRITE    0x30u  /* WRITE SECTORS (with retry)*/
#define ATA_CMD_FLUSH    0xE7u  /* FLUSH CACHE               */
#define ATA_CMD_IDENTIFY 0xECu  /* IDENTIFY DEVICE           */

/* IDENTIFY response word offsets (256 × uint16_t) */
#define ATA_ID_SERIAL   10  /* 10 words = 20 chars (byte-swapped) */
#define ATA_ID_MODEL    27  /* 20 words = 40 chars (byte-swapped) */
#define ATA_ID_LBA28    60  /* 2 words  = uint32_t sector count   */

/* Maximum number of polling iterations before declaring a timeout. */
#define ATA_POLL_LIMIT  0x200000

/* ---------------------------------------------------------------------------
 * Driver state
 * --------------------------------------------------------------------------- */

static ata_drive_t drives[ATA_MAX_DRIVES];
static int         drive_count = 0;

/* ---------------------------------------------------------------------------
 * Internal helpers
 * --------------------------------------------------------------------------- */

/*
 * ata_400ns_delay — read the alternate-status register four times to give
 * the drive at least 400 ns to update its status after a command or drive
 * select.  Each inb on a slow ISA-speed register takes ~100 ns.
 */
static inline void ata_400ns_delay(void)
{
    inb(ATA_CTRL);
    inb(ATA_CTRL);
    inb(ATA_CTRL);
    inb(ATA_CTRL);
}

/*
 * ata_wait_busy — spin until BSY clears.
 * Returns 0 on success, -1 if the drive never became ready (timeout).
 */
static int ata_wait_busy(void)
{
    for (int i = 0; i < ATA_POLL_LIMIT; i++) {
        if (!(inb(ATA_BASE + ATA_REG_STATUS) & ATA_SR_BSY))
            return 0;
    }
    return -1;
}

/*
 * ata_wait_drq — spin until DRQ is set, or ERR/DF is set.
 * Returns 0 when DRQ is ready, -1 on drive fault or error.
 */
static int ata_wait_drq(void)
{
    for (int i = 0; i < ATA_POLL_LIMIT; i++) {
        uint8_t st = inb(ATA_BASE + ATA_REG_STATUS);
        if (st & (ATA_SR_ERR | ATA_SR_DF))
            return -1;
        if (st & ATA_SR_DRQ)
            return 0;
    }
    return -1;
}

/*
 * ata_fix_string — IDENTIFY data strings are stored as big-endian pairs of
 * bytes within each 16-bit word.  Swap them into normal byte order, then
 * strip trailing ASCII spaces.  `dst` must be at least nwords*2+1 bytes.
 */
static void ata_fix_string(char *dst, const uint16_t *words, int nwords)
{
    for (int i = 0; i < nwords; i++) {
        dst[i * 2]     = (char)(words[i] >> 8);
        dst[i * 2 + 1] = (char)(words[i] & 0xFF);
    }
    dst[nwords * 2] = '\0';

    /* Trim trailing spaces. */
    for (int i = nwords * 2 - 1; i >= 0 && dst[i] == ' '; i--)
        dst[i] = '\0';
}

/*
 * ata_probe_drive — attempt to IDENTIFY the drive at the given index.
 * Populates drives[idx] and returns 1 if a drive is present, 0 otherwise.
 */
static int ata_probe_drive(int idx)
{
    ata_drive_t *drv = &drives[idx];
    uint8_t sel = (idx == ATA_DRIVE_MASTER) ? ATA_SEL_MASTER : ATA_SEL_SLAVE;

    /* Select the drive. */
    outb(ATA_BASE + ATA_REG_DRIVE, sel);
    ata_400ns_delay();

    /* Clear sector count and LBA registers (required before IDENTIFY). */
    outb(ATA_BASE + ATA_REG_SECCOUNT, 0);
    outb(ATA_BASE + ATA_REG_LBA_LO,   0);
    outb(ATA_BASE + ATA_REG_LBA_MID,  0);
    outb(ATA_BASE + ATA_REG_LBA_HI,   0);

    /* Issue IDENTIFY. */
    outb(ATA_BASE + ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
    ata_400ns_delay();

    /* If status reads 0x00, no drive is present on this position. */
    uint8_t st = inb(ATA_BASE + ATA_REG_STATUS);
    if (st == 0)
        return 0;

    /* Wait for BSY to clear. */
    if (ata_wait_busy() < 0)
        return 0;

    /*
     * Check LBA mid/high registers.  An ATAPI device sets these to
     * 0x14/0xEB; any other non-zero value is an anomaly.  Either way,
     * skip: we only support plain ATA in this driver.
     */
    if (inb(ATA_BASE + ATA_REG_LBA_MID) || inb(ATA_BASE + ATA_REG_LBA_HI))
        return 0;

    /* Wait for DRQ (data ready to be read). */
    if (ata_wait_drq() < 0)
        return 0;

    /* Read the 256-word IDENTIFY response. */
    uint16_t buf[256];
    for (int i = 0; i < 256; i++)
        buf[i] = inw(ATA_BASE + ATA_REG_DATA);

    /* Extract the 28-bit LBA sector count (words 60–61, little-endian). */
    drv->sectors = (uint32_t)buf[ATA_ID_LBA28] |
                   ((uint32_t)buf[ATA_ID_LBA28 + 1] << 16);

    /* Extract model and serial strings (byte-swapped within each word). */
    ata_fix_string(drv->model,  &buf[ATA_ID_MODEL],  20);
    ata_fix_string(drv->serial, &buf[ATA_ID_SERIAL], 10);

    drv->present = 1;
    return 1;
}

/* ---------------------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------------------- */

void ata_init(void)
{
    drive_count = 0;

    for (int i = 0; i < ATA_MAX_DRIVES; i++) {
        drives[i].present = 0;
        drives[i].sectors = 0;
        drives[i].model[0]  = '\0';
        drives[i].serial[0] = '\0';

        if (ata_probe_drive(i)) {
            drive_count++;
            KLOG("ata: drive ");
            KLOG_DEC(i);
            KLOG(" present: ");
            KLOG(drives[i].model);
            KLOG(" sectors=");
            KLOG_DEC(drives[i].sectors);
            KLOG("\n");
        } else {
            KLOG("ata: drive ");
            KLOG_DEC(i);
            KLOG(" not present\n");
        }
    }
}

int ata_read(int idx, uint32_t lba, uint8_t count, void *buf)
{
    if (idx < 0 || idx >= ATA_MAX_DRIVES || !drives[idx].present)
        return -1;
    if (count == 0)
        return 0;

    uint8_t sel = (idx == ATA_DRIVE_MASTER) ? ATA_LBA_MASTER : ATA_LBA_SLAVE;

    if (ata_wait_busy() < 0)
        return -1;

    /* Set up 28-bit LBA addressing. */
    outb(ATA_BASE + ATA_REG_DRIVE,    sel | ((lba >> 24) & 0x0Fu));
    outb(ATA_BASE + ATA_REG_SECCOUNT, count);
    outb(ATA_BASE + ATA_REG_LBA_LO,   (uint8_t)(lba & 0xFFu));
    outb(ATA_BASE + ATA_REG_LBA_MID,  (uint8_t)((lba >> 8)  & 0xFFu));
    outb(ATA_BASE + ATA_REG_LBA_HI,   (uint8_t)((lba >> 16) & 0xFFu));
    outb(ATA_BASE + ATA_REG_COMMAND,  ATA_CMD_READ);

    uint16_t *ptr = (uint16_t *)buf;
    for (int s = 0; s < count; s++) {
        if (ata_wait_drq() < 0)
            return -1;
        /* Read 256 16-bit words (= 512 bytes) per sector. */
        for (int i = 0; i < 256; i++)
            ptr[s * 256 + i] = inw(ATA_BASE + ATA_REG_DATA);
        ata_400ns_delay();
    }

    return 0;
}

int ata_write(int idx, uint32_t lba, uint8_t count, const void *buf)
{
    if (idx < 0 || idx >= ATA_MAX_DRIVES || !drives[idx].present)
        return -1;
    if (count == 0)
        return 0;

    uint8_t sel = (idx == ATA_DRIVE_MASTER) ? ATA_LBA_MASTER : ATA_LBA_SLAVE;

    if (ata_wait_busy() < 0)
        return -1;

    /* Set up 28-bit LBA addressing. */
    outb(ATA_BASE + ATA_REG_DRIVE,    sel | ((lba >> 24) & 0x0Fu));
    outb(ATA_BASE + ATA_REG_SECCOUNT, count);
    outb(ATA_BASE + ATA_REG_LBA_LO,   (uint8_t)(lba & 0xFFu));
    outb(ATA_BASE + ATA_REG_LBA_MID,  (uint8_t)((lba >> 8)  & 0xFFu));
    outb(ATA_BASE + ATA_REG_LBA_HI,   (uint8_t)((lba >> 16) & 0xFFu));
    outb(ATA_BASE + ATA_REG_COMMAND,  ATA_CMD_WRITE);

    const uint16_t *ptr = (const uint16_t *)buf;
    for (int s = 0; s < count; s++) {
        if (ata_wait_drq() < 0)
            return -1;
        /* Write 256 16-bit words per sector. */
        for (int i = 0; i < 256; i++)
            outw(ATA_BASE + ATA_REG_DATA, ptr[s * 256 + i]);
        ata_400ns_delay();
    }

    /* Flush the drive's write cache. */
    outb(ATA_BASE + ATA_REG_COMMAND, ATA_CMD_FLUSH);
    ata_wait_busy();

    return 0;
}

const ata_drive_t *ata_get_drive(int idx)
{
    if (idx < 0 || idx >= ATA_MAX_DRIVES)
        return NULL;
    return &drives[idx];
}

int ata_drive_count(void)
{
    return drive_count;
}
