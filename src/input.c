/* Keyboard, in the shape the game expects.
 *
 * The X16 build reads keys through KERNAL GETIN, which returns one PETSCII code
 * per call and zero when the buffer is empty. Every comparison in the ported
 * assembly is against those codes -- STANDARD_CONTROLS is literally
 * {73, 75, 74, 76, ...} for I K J L -- so the cheapest way to keep that code
 * unchanged is to keep producing the same codes.
 *
 * The RIA gives a 32-byte bit array of USB HID keycodes instead: bit N is set
 * while the key with HID code N is down. So this file does three things GETIN
 * did for free -- translate, detect the press rather than the hold, and repeat.
 *
 * Polling happens inside plat_getin, once per frame, rather than from a game
 * loop. Reading the bit array means reading XRAM through a portal, which the
 * interrupt handler must not do, and the game asks for keys from places that
 * are not the main loop -- USER_SELECT_OBJECT drives its own loop while the
 * selection cursor is up. Keying the poll off the interrupt's frame counter
 * makes it correct wherever it is called from.
 */
#include <rp6502.h>

#include "game/game.h"
#include "input.h"
#include "xram.h"

/* Bits 0-3 of byte 0 are special: bit 0 means no key is pressed, bits 1-3 are
 * the lock LEDs. Real keys start at HID code 4. */
#define key_down(bits, code) ((bits)[(code) >> 3] & (1 << ((code) & 7)))
#define HID_NONE_DOWN 0x01

#define HID_A     0x04
#define HID_Z     0x1D
#define HID_1     0x1E
#define HID_0     0x27
#define HID_ENTER 0x28
#define HID_ESC   0x29
#define HID_TAB   0x2B
#define HID_SPACE 0x2C
#define HID_F1    0x3A
#define HID_F12   0x45
#define HID_RIGHT 0x4F
#define HID_LEFT  0x50
#define HID_DOWN  0x51
#define HID_UP    0x52

/* The key repeat the X16 got from the KERNAL: a long wait before the first
 * repeat, then a short one. Measured in frames. */
#define REPEAT_DELAY 20
#define REPEAT_RATE   7

static unsigned char now[32], before[32];
static unsigned char queue[8], q_head, q_tail;
static unsigned char held_code, held_timer;

/* HID keycode -> the code the game compares against. These are the PETSCII
 * values KERNAL GETIN produced, so STANDARD_CONTROLS and every CMP in the
 * ported assembly work unchanged. */
static unsigned char translate(unsigned char hid)
{
    if (hid >= HID_A && hid <= HID_Z)
        return (unsigned char)('A' + (hid - HID_A));    /* letters, uppercase */
    if (hid >= HID_1 && hid <= HID_0)
        return (unsigned char)(hid == HID_0 ? '0' : '1' + (hid - HID_1));
    switch (hid) {
    case HID_SPACE: return 32;
    case HID_ENTER: return 13;
    case HID_TAB:   return 9;
    case HID_ESC:   return 3;      /* the X16's run/stop */
    case HID_RIGHT: return 0x1D;   /* PETSCII cursor right */
    case HID_LEFT:  return 0x9D;
    case HID_DOWN:  return 0x11;
    case HID_UP:    return 0x91;
    case HID_F1:    return 133;
    case HID_F1 + 1: return 137;   /* F2, as the X16 numbered them */
    case HID_F1 + 2: return 134;   /* F3 */
    case HID_F1 + 3: return 138;   /* F4 */
    default:        return 0;
    }
}

static void push(unsigned char code)
{
    unsigned char next = (unsigned char)((q_tail + 1) & 7);
    if (code && next != q_head) {       /* a full queue drops, as a real one does */
        queue[q_tail] = code;
        q_tail = next;
    }
}

static unsigned char polled_frame;

void input_init(void)
{
    unsigned char i;
    for (i = 0; i < 32; i++)
        before[i] = 0;
    q_head = q_tail = held_code = held_timer = 0;
    xreg_ria_keyboard(XR_KEYBOARD);
    polled_frame = IRQ_FRAME;
}

/* Queues a code for every key that went down since the last call, and one more
 * each time the repeat timer expires on the key still held. */
static void input_poll(void)
{
    unsigned char i, b, hid, code;

    RIA.addr0 = XR_KEYBOARD;
    RIA.step0 = 1;
    for (i = 0; i < 32; i++)
        now[i] = RIA.rw0;

    for (i = 0; i < 32; i++) {
        b = (unsigned char)(now[i] & ~before[i]);        /* newly pressed */
        if (!b)
            continue;
        for (hid = 0; hid < 8; hid++) {
            if (!(b & (1 << hid)))
                continue;
            /* Byte 0 bits 0-3 are the no-key flag and the lock LEDs. */
            if (i == 0 && hid < 4)
                continue;
            code = translate((unsigned char)((i << 3) | hid));
            if (code) {
                push(code);
                held_code = code;
                held_timer = REPEAT_DELAY;
            }
        }
        before[i] = now[i];
    }
    for (i = 0; i < 32; i++)
        before[i] = now[i];

    if (now[0] & HID_NONE_DOWN) {
        held_code = 0;                  /* nothing held: no repeat pending */
    } else if (held_code) {
        if (held_timer)
            held_timer--;
        else {
            push(held_code);
            held_timer = REPEAT_RATE;
        }
    }
}

/* What KERNAL GETIN returned: the next key code, or zero if none is waiting.
 *
 * Polls the hardware at most once per frame, so calling this in a tight loop
 * neither misses a keypress nor turns one into a stream. */
unsigned char plat_getin(void)
{
    unsigned char code;
    if (polled_frame != IRQ_FRAME) {
        polled_frame = IRQ_FRAME;
        input_poll();
    }
    if (q_head == q_tail)
        return 0;
    code = queue[q_head];
    q_head = (unsigned char)((q_head + 1) & 7);
    return code;
}

void plat_clear_key_buffer(void)
{
    q_head = q_tail = 0;
    held_code = 0;
}
