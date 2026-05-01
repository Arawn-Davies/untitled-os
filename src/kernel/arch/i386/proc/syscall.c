/*
 * syscall.c — int 0x80 syscall dispatcher (Linux i386 ABI subset).
 *
 * Supported calls:
 *   SYS_EXIT  (1)  — terminate current task
 *   SYS_READ  (3)  — read from fd (0=keyboard, 3+ = open file)
 *   SYS_WRITE (4)  — write to fd  (1/2=VGA, 3+ = open file [NYI])
 *   SYS_OPEN  (5)  — open a VFS path, returns fd
 *   SYS_CLOSE (6)  — close a file fd
 *   SYS_BRK   (45) — expand/query user-space heap break
 *   SYS_YIELD (158)— cooperative yield
 *   SYS_DEBUG (100)— debug checkpoint (Makar extension)
 *
 * File descriptors:
 *   0 = stdin  (keyboard, blocking via keyboard_getchar)
 *   1 = stdout (VGA terminal)
 *   2 = stderr (VGA terminal, same as stdout)
 *   3–10 = open files (SYS_OPEN reads the whole file into a heap buffer)
 *
 * Interrupts stay disabled for the duration of the syscall (the isr_common_stub
 * begins with cli). keyboard_getchar() task_yield()s internally so other tasks
 * — which have IF=1 in their saved EFLAGS — can receive IRQ1 and fill the
 * keyboard ring buffer while this task waits.
 */

#include <kernel/syscall.h>
#include <kernel/isr.h>
#include <kernel/task.h>
#include <kernel/tty.h>
#include <kernel/keyboard.h>
#include <kernel/shell.h>
#include <kernel/vfs.h>
#include <kernel/heap.h>
#include <kernel/vmm.h>
#include <kernel/pmm.h>
#include <kernel/serial.h>
#include <string.h>
#include <kernel/ktest.h>

/* -------------------------------------------------------------------------
 * File descriptor table (global — only one user process runs at a time)
 * ------------------------------------------------------------------------- */

#define FD_SLOT_COUNT  8   /* fds 3..10 */
#define FD_FILE_BASE   3

typedef struct {
    int      valid;
    uint8_t *data;     /* kmalloc'd, SYSCALL_FILE_MAX cap */
    uint32_t size;     /* total bytes in data              */
    uint32_t pos;      /* current read position            */
} fd_slot_t;

static fd_slot_t s_fds[FD_SLOT_COUNT];

/* Convert a user-visible fd (>= FD_FILE_BASE) to a slot index, or -1. */
static int fd_to_slot(int fd)
{
    int idx = fd - FD_FILE_BASE;
    if (idx < 0 || idx >= FD_SLOT_COUNT)
        return -1;
    if (!s_fds[idx].valid)
        return -1;
    return idx;
}

/* Find the first free slot index, or -1 if the table is full. */
static int alloc_slot(void)
{
    for (int i = 0; i < FD_SLOT_COUNT; i++) {
        if (!s_fds[i].valid)
            return i;
    }
    return -1;
}

void syscall_reset_fds(void)
{
    for (int i = 0; i < FD_SLOT_COUNT; i++) {
        if (s_fds[i].valid) {
            kfree(s_fds[i].data);
            s_fds[i].valid = 0;
            s_fds[i].data  = NULL;
            s_fds[i].size  = 0;
            s_fds[i].pos   = 0;
        }
    }
}

/* -------------------------------------------------------------------------
 * Checkpoint tracking
 * ------------------------------------------------------------------------- */

volatile uint32_t g_ring3_last_cp = 0;

/* -------------------------------------------------------------------------
 * syscall_dispatch
 * ------------------------------------------------------------------------- */

void syscall_dispatch(registers_t *regs)
{
    switch (regs->eax) {

    /* ------------------------------------------------------------------
     * SYS_EXIT(1): terminate the calling task.
     * EBX = exit status (ignored for now).
     * ------------------------------------------------------------------ */
    case SYS_EXIT:
        task_exit();   /* does not return */
        break;

    /* ------------------------------------------------------------------
     * SYS_READ(3): read bytes from a file descriptor.
     * EBX = fd, ECX = buf, EDX = len
     * Returns: bytes read (EAX), 0 on EOF, (uint32_t)-1 on error.
     *
     * fd 0 (stdin): blocks via keyboard_getchar() until len bytes are
     * available or '\n' is seen.
     * fd 3+: reads from the in-memory file buffer opened by SYS_OPEN.
     * ------------------------------------------------------------------ */
    case SYS_READ: {
        int      fd  = (int)regs->ebx;
        char    *buf = (char *)(uintptr_t)regs->ecx;
        uint32_t len = regs->edx;

        if (!buf || len == 0) { regs->eax = 0; break; }

        if (fd == FD_STDIN) {
            /* Line-buffered stdin with echo, backspace, and cursor editing. */
            static char s_stdin_line[256];
            uint32_t cap = (len < sizeof(s_stdin_line)) ? len : (uint32_t)sizeof(s_stdin_line);
            shell_readline(s_stdin_line, (size_t)cap);
            uint32_t n = (uint32_t)strlen(s_stdin_line);
            /* Append '\n' so callers see a complete line (like a real terminal). */
            if (n < cap - 1) { s_stdin_line[n++] = '\n'; s_stdin_line[n] = '\0'; }
            if (n > len) n = len;
            memcpy(buf, s_stdin_line, n);
            regs->eax = n;
        } else {
            int idx = fd_to_slot(fd);
            if (idx < 0) { regs->eax = (uint32_t)-1; break; }

            fd_slot_t *sl = &s_fds[idx];
            uint32_t avail = sl->size - sl->pos;
            uint32_t n     = (len < avail) ? len : avail;
            if (n > 0) {
                memcpy(buf, sl->data + sl->pos, n);
                sl->pos += n;
            }
            regs->eax = n;   /* 0 signals EOF when avail was 0 */
        }
        break;
    }

    /* ------------------------------------------------------------------
     * SYS_WRITE(4): write bytes to a file descriptor.
     * EBX = fd, ECX = buf, EDX = len
     * Returns: bytes written (EAX), (uint32_t)-1 on error.
     *
     * fd 1 (stdout) and fd 2 (stderr) both write to the VGA terminal.
     * Writing to file fds is not yet implemented.
     * ------------------------------------------------------------------ */
    case SYS_WRITE: {
        int         fd  = (int)regs->ebx;
        const char *buf = (const char *)(uintptr_t)regs->ecx;
        uint32_t    len = regs->edx;

        if (!buf) { regs->eax = (uint32_t)-1; break; }

        if (fd == FD_STDOUT || fd == FD_STDERR) {
            if (!ktest_muted) {
                for (uint32_t i = 0; i < len; i++)
                    t_putchar(buf[i]);
            }
            regs->eax = len;
        } else {
            /* File write not yet implemented. */
            regs->eax = (uint32_t)-1;
        }
        break;
    }

    /* ------------------------------------------------------------------
     * SYS_OPEN(5): open a VFS path and return a file descriptor.
     * EBX = path (NUL-terminated string in user space)
     * ECX = flags (O_RDONLY=0; other modes not yet supported)
     * Returns: fd >= FD_FILE_BASE on success, (uint32_t)-1 on error.
     *
     * The entire file is read into a heap buffer (cap: SYSCALL_FILE_MAX).
     * ------------------------------------------------------------------ */
    case SYS_OPEN: {
        const char *path  = (const char *)(uintptr_t)regs->ebx;
        /* flags (ecx) reserved for future use */

        if (!path) { regs->eax = (uint32_t)-1; break; }

        int idx = alloc_slot();
        if (idx < 0) { regs->eax = (uint32_t)-1; break; }  /* too many open files */

        uint8_t *buf = (uint8_t *)kmalloc(SYSCALL_FILE_MAX);
        if (!buf)   { regs->eax = (uint32_t)-1; break; }

        uint32_t out_sz = 0;
        if (vfs_read_file(path, buf, SYSCALL_FILE_MAX, &out_sz) != 0) {
            kfree(buf);
            regs->eax = (uint32_t)-1;
            break;
        }

        s_fds[idx].valid = 1;
        s_fds[idx].data  = buf;
        s_fds[idx].size  = out_sz;
        s_fds[idx].pos   = 0;

        regs->eax = (uint32_t)(idx + FD_FILE_BASE);
        break;
    }

    /* ------------------------------------------------------------------
     * SYS_CLOSE(6): close a file descriptor and free its buffer.
     * EBX = fd
     * Returns: 0 on success, (uint32_t)-1 on error.
     * ------------------------------------------------------------------ */
    case SYS_CLOSE: {
        int fd  = (int)regs->ebx;
        int idx = fd_to_slot(fd);
        if (idx < 0) { regs->eax = (uint32_t)-1; break; }

        kfree(s_fds[idx].data);
        s_fds[idx].valid = 0;
        s_fds[idx].data  = NULL;
        s_fds[idx].size  = 0;
        s_fds[idx].pos   = 0;

        regs->eax = 0;
        break;
    }

    /* ------------------------------------------------------------------
     * SYS_BRK(45): set or query the user-space heap break.
     * EBX = requested new break (0 = query current break)
     * Returns: current break after the call.
     *
     * Pages are allocated from the PMM and mapped writable+user in the
     * current task's page directory.  The break only ever grows.
     * ------------------------------------------------------------------ */
    case SYS_BRK: {
        uint32_t  new_brk = regs->ebx;
        task_t   *t       = task_current();

        if (new_brk == 0 || new_brk <= t->user_brk) {
            regs->eax = t->user_brk;
            break;
        }

        /* Align the current break up to the next page boundary, then map
         * all pages needed to reach new_brk. */
        uint32_t cur_page = (t->user_brk + 0xFFFu) & ~0xFFFu;
        uint32_t new_page = (new_brk     + 0xFFFu) & ~0xFFFu;

        for (uint32_t va = cur_page; va < new_page; va += 0x1000u) {
            uint32_t phys = pmm_alloc_frame();
            if (phys == PMM_ALLOC_ERROR) {
                /* Return what we managed to allocate so far. */
                regs->eax = t->user_brk;
                goto brk_done;
            }
            memset((void *)phys, 0, 0x1000u);
            vmm_map_page(t->page_dir, va, phys,
                         VMM_FLAG_USER | VMM_FLAG_WRITABLE);
        }
        t->user_brk = new_brk;
        regs->eax   = new_brk;
    brk_done:
        break;
    }

    /* ------------------------------------------------------------------
     * SYS_YIELD(158): voluntarily give up the CPU.
     * ------------------------------------------------------------------ */
    case SYS_YIELD:
        task_yield();
        break;

    /* ------------------------------------------------------------------
     * SYS_DEBUG(100): Makar extension — print a debug checkpoint.
     * EBX = uint32 checkpoint value, written to VGA + serial.
     * ------------------------------------------------------------------ */
    case SYS_DEBUG:
        g_ring3_last_cp = regs->ebx;
        Serial_WriteString("[ring3] CP: 0x");
        Serial_WriteHex(regs->ebx);
        Serial_WriteString("\n");
        if (!ktest_muted) {
            t_writestring("[ring3] CP: 0x");
            t_hex(regs->ebx);
            t_putchar('\n');
        }
        break;

    default:
        /* Unknown syscall — return -ENOSYS. */
        regs->eax = (uint32_t)-38;   /* -ENOSYS */
        break;
    }
}

void syscall_init(void)
{
    register_interrupt_handler(0x80, syscall_dispatch);
}
