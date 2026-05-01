//
// keyboard.c -- PS/2 keyboard driver.
// Handles IRQ1, translates scan-code set 1 to ASCII (US QWERTY layout),
// tracks shift/caps-lock state, and provides per-task input queues.
//
// Phase 2 additions:
//   - Per-task 64-byte ring buffers (up to KB_TASK_SLOTS tasks).
//   - Ctrl-A two-key prefix: Ctrl-A,U → top pane; Ctrl-A,J → bottom pane.
//   - keyboard_bind_pane() / keyboard_focus_pane() / keyboard_set_focus().
//   - keyboard_getchar() dequeues from the calling task's bound slot.
//

#include <kernel/keyboard.h>
#include <kernel/isr.h>
#include <kernel/asm.h>
#include <kernel/task.h>

#define PS2_DATA_PORT   0x60
#define PS2_STATUS_PORT 0x64

// Scan-code set 1: key-release codes have bit 7 set (scan | 0x80).
#define SC_LSHIFT       0x2A
#define SC_RSHIFT       0x36
#define SC_LSHIFT_REL   (SC_LSHIFT | 0x80)
#define SC_RSHIFT_REL   (SC_RSHIFT | 0x80)
#define SC_LCTRL        0x1D
#define SC_LCTRL_REL    (SC_LCTRL | 0x80)
#define SC_CAPSLOCK     0x3A
#define SC_BACKSPACE    0x0E
#define SC_ENTER        0x1C

// ---------------------------------------------------------------------------
// Scan-code → ASCII tables (unshifted and shifted), indexed by scan code.
// Only scan-code set 1, codes 0x00-0x58 are mapped; everything else → 0.
// ---------------------------------------------------------------------------

static const char sc_ascii_lower[89] = {
    0,    0,   '1', '2', '3', '4', '5', '6',  // 0x00-0x07
    '7', '8',  '9', '0', '-', '=', '\b', '\t', // 0x08-0x0F
    'q', 'w',  'e', 'r', 't', 'y', 'u', 'i',  // 0x10-0x17
    'o', 'p',  '[', ']', '\n', 0,  'a', 's',  // 0x18-0x1F
    'd', 'f',  'g', 'h', 'j', 'k', 'l', ';',  // 0x20-0x27
    '\'', '`', 0,  '\\','z', 'x', 'c', 'v',   // 0x28-0x2F
    'b', 'n',  'm', ',', '.', '/', 0,   '*',   // 0x30-0x37
    0,   ' ',  0,   0,   0,   0,   0,   0,     // 0x38-0x3F
    0,   0,    0,   0,   0,   0,   0,   '7',   // 0x40-0x47
    '8', '9',  '-', '4', '5', '6', '+', '1',   // 0x48-0x4F
    '2', '3',  '0', '.', 0,   0,   0,   0,     // 0x50-0x57
    0                                           // 0x58
};

static const char sc_ascii_upper[89] = {
    0,    0,   '!', '@', '#', '$', '%', '^',   // 0x00-0x07
    '&', '*',  '(', ')', '_', '+', '\b', '\t', // 0x08-0x0F
    'Q', 'W',  'E', 'R', 'T', 'Y', 'U', 'I',  // 0x10-0x17
    'O', 'P',  '{', '}', '\n', 0,  'A', 'S',  // 0x18-0x1F
    'D', 'F',  'G', 'H', 'J', 'K', 'L', ':',  // 0x20-0x27
    '"', '~',  0,   '|', 'Z', 'X', 'C', 'V',  // 0x28-0x2F
    'B', 'N',  'M', '<', '>', '?', 0,   '*',   // 0x30-0x37
    0,   ' ',  0,   0,   0,   0,   0,   0,     // 0x38-0x3F
    0,   0,    0,   0,   0,   0,   0,   '7',   // 0x40-0x47
    '8', '9',  '-', '4', '5', '6', '+', '1',   // 0x48-0x4F
    '2', '3',  '0', '.', 0,   0,   0,   0,     // 0x50-0x57
    0                                           // 0x58
};

// ---------------------------------------------------------------------------
// Global fallback ring (used before any task registers, or when pool is full)
// ---------------------------------------------------------------------------

#define KB_BUF_SIZE 256

static volatile uint8_t kb_buf[KB_BUF_SIZE];
static volatile uint8_t kb_buf_head = 0;
static volatile uint8_t kb_buf_tail = 0;

static inline uint8_t buf_count(void)
{
    return (uint8_t)(kb_buf_head - kb_buf_tail);
}
static inline void buf_push(char c)
{
    if (buf_count() < KB_BUF_SIZE - 1)
        kb_buf[kb_buf_head++] = (uint8_t)c;
}
static inline char buf_pop(void)
{
    return (char)kb_buf[kb_buf_tail++];
}

// ---------------------------------------------------------------------------
// Per-task input slots
// ---------------------------------------------------------------------------

#define KB_TASK_SLOTS  4
#define KB_SLOT_BUF    64

typedef struct {
    task_t  *owner;
    uint8_t  buf[KB_SLOT_BUF];
    uint8_t  head;
    uint8_t  tail;
} kb_slot_t;

static kb_slot_t kb_slots[KB_TASK_SLOTS];
static int       kb_nslots  = 0;
static task_t   *kb_focused = NULL;        /* task that receives new input    */
static int       kb_prefix  = 0;           /* 1 = Ctrl-A prefix pending       */
static task_t   *kb_pane[2] = {NULL, NULL};/* [KB_PANE_TOP/BOTTOM] task ptrs  */

static inline int slot_empty(const kb_slot_t *s)
{
    return s->head == s->tail;
}
static inline uint8_t slot_count(const kb_slot_t *s)
{
    return (uint8_t)(s->head - s->tail);
}
static inline void slot_push(kb_slot_t *s, char c)
{
    if (slot_count(s) < KB_SLOT_BUF - 1)
        s->buf[s->head++] = (uint8_t)c;
}
static inline char slot_pop(kb_slot_t *s)
{
    return (char)s->buf[s->tail++];
}

// Route a character to the focused task's slot; fall back to the global ring.
static void kb_route(char c)
{
    if (kb_focused) {
        for (int i = 0; i < kb_nslots; i++) {
            if (kb_slots[i].owner == kb_focused) {
                slot_push(&kb_slots[i], c);
                return;
            }
        }
    }
    buf_push(c);
}

// Find an existing slot for t, or allocate a new one.
// The first task to register automatically becomes the focused task.
// Returns slot index, or -1 if the pool is exhausted.
static int kb_find_or_register(task_t *t)
{
    for (int i = 0; i < kb_nslots; i++)
        if (kb_slots[i].owner == t)
            return i;
    if (kb_nslots >= KB_TASK_SLOTS)
        return -1;
    int i = kb_nslots++;
    kb_slots[i].owner = t;
    kb_slots[i].head  = 0;
    kb_slots[i].tail  = 0;
    if (!kb_focused)
        kb_focused = t;
    return i;
}

// ---------------------------------------------------------------------------
// Modifier state
// ---------------------------------------------------------------------------

static volatile int shift_pressed = 0;
static volatile int caps_lock_on  = 0;
static volatile int ctrl_pressed  = 0;
static volatile int extended_key  = 0;
static volatile int g_sigint      = 0;

// ---------------------------------------------------------------------------
// IRQ1 handler
// ---------------------------------------------------------------------------

static void keyboard_irq_handler(registers_t *regs)
{
    (void)regs;

    uint8_t sc = inb(PS2_DATA_PORT);

    if (sc == 0xE0) { extended_key = 1; return; }

    if (extended_key) {
        extended_key = 0;
        if (!(sc & 0x80)) {
            if (sc == 0x48) { kb_route(KEY_ARROW_UP);    return; }
            if (sc == 0x50) { kb_route(KEY_ARROW_DOWN);  return; }
            if (sc == 0x4B) { kb_route(KEY_ARROW_LEFT);  return; }
            if (sc == 0x4D) { kb_route(KEY_ARROW_RIGHT); return; }
        }
        return;
    }

    if (sc == SC_LSHIFT_REL || sc == SC_RSHIFT_REL) { shift_pressed = 0; return; }
    if (sc == SC_LCTRL_REL)                          { ctrl_pressed  = 0; return; }
    if (sc & 0x80)                                     return;
    if (sc == SC_LSHIFT || sc == SC_RSHIFT)          { shift_pressed = 1; return; }
    if (sc == SC_LCTRL)                              { ctrl_pressed  = 1; return; }
    if (sc == SC_CAPSLOCK) { caps_lock_on = !caps_lock_on; return; }

    if (sc >= sizeof(sc_ascii_lower)) return;

    char lower_c  = sc_ascii_lower[sc];
    int  is_letter = (lower_c >= 'a' && lower_c <= 'z');
    int  use_upper = is_letter ? (shift_pressed ^ caps_lock_on) : shift_pressed;
    char c         = use_upper ? sc_ascii_upper[sc] : sc_ascii_lower[sc];

    if (c == 0) return;

    // Ctrl+A → arm the pane-switch prefix; never passes to any task.
    if (ctrl_pressed && lower_c == 'a') {
        kb_prefix = 1;
        return;
    }

    // Ctrl+C → cancel signal (0x03 falls in the arrow-sentinel skip range;
    // handle it explicitly so it always reaches the focused task).
    if (ctrl_pressed && lower_c == 'c') {
        g_sigint = 1;
        kb_route('\x03');
        return;
    }

    // Ctrl-A prefix pending: next key (plain or chorded) is a pane command.
    if (kb_prefix) {
        kb_prefix = 0;
        if (lower_c == 'u') keyboard_focus_pane(KB_PANE_TOP);
        else if (lower_c == 'j') keyboard_focus_pane(KB_PANE_BOTTOM);
        // all other keys silently cancel the prefix
        return;
    }

    // Ctrl+letter → control code.
    // Ctrl+A and Ctrl+C are already handled above; remaining codes are routed directly.
    if (ctrl_pressed && is_letter) {
        char ctrl_code = (char)(lower_c - 'a' + 1);
        kb_route(ctrl_code);
        return;
    }

    kb_route(c);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void keyboard_init(void)
{
    register_interrupt_handler(IRQ1, keyboard_irq_handler);
}

void keyboard_bind_pane(int pane_id, task_t *t)
{
    if (pane_id < 0 || pane_id > 1 || !t) return;
    kb_pane[pane_id] = t;
    kb_find_or_register(t);
}

void keyboard_focus_pane(int pane_id)
{
    if (pane_id < 0 || pane_id > 1) return;
    if (kb_pane[pane_id])
        kb_focused = kb_pane[pane_id];
}

void keyboard_set_focus(task_t *t)
{
    kb_focused = t;
}

int keyboard_sigint_consume(void)
{
    if (!g_sigint) return 0;
    g_sigint = 0;
    return 1;
}

char keyboard_poll(void)
{
    task_t *me = task_current();
    if (me) {
        for (int i = 0; i < kb_nslots; i++) {
            if (kb_slots[i].owner == me)
                return slot_empty(&kb_slots[i]) ? 0 : slot_pop(&kb_slots[i]);
        }
    }
    return buf_count() ? buf_pop() : 0;
}

char keyboard_getchar(void)
{
    task_t *me = task_current();
    if (me) {
        int slot = kb_find_or_register(me);
        if (slot >= 0) {
            while (slot_empty(&kb_slots[slot])) {
                task_yield();
                asm volatile("pause");
            }
            return slot_pop(&kb_slots[slot]);
        }
    }
    // Fallback: global ring (slot pool exhausted or called before tasking).
    while (buf_count() == 0) {
        task_yield();
        asm volatile("pause");
    }
    return buf_pop();
}
