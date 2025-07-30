#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#define KB_DATA_PORT    0x60
#define KB_STATUS_PORT  0x64
#define KB_COMMAND_PORT 0x64

#define KB_STATUS_OUTPUT_FULL   0x01
#define KB_STATUS_INPUT_FULL    0x02
#define KB_ENABLE_KEYBOARD      0xAE

typedef enum {
    KEY_UNKNOWN = 0,
    KEY_ESC = 1,
    KEY_CTRL = 2,
    KEY_LSHIFT = 3,
    KEY_RSHIFT = 4,
    KEY_ALT = 5,
    KEY_CAPS = 6,
    KEY_F1 = 128,
    KEY_F2 = 129,
    KEY_F3 = 130,
    KEY_F4 = 131,
    KEY_F5 = 132,
    KEY_F6 = 133,
    KEY_F7 = 134,
    KEY_F8 = 135,
    KEY_F9 = 136,
    KEY_F10 = 137,
    KEY_F11 = 138,
    KEY_F12 = 139,
    KEY_HOME = 140,
    KEY_END = 141,
    KEY_PGUP = 142,
    KEY_PGDOWN = 143,
    KEY_UP = 144,
    KEY_DOWN = 145,
    KEY_LEFT = 146,
    KEY_RIGHT = 147,
    KEY_INSERT = 148,
    KEY_DELETE = 149,
    KEY_BACKSPACE = '\b',
    KEY_TAB = '\t',
    KEY_ENTER = '\n',
    KEY_SPACE = ' '
} special_key_t;

static bool caps_lock_on = false;
static bool shift_pressed = false;
static bool ctrl_pressed = false;
static bool alt_pressed = false;
static bool extended_scancode = false;

static const uint8_t scancode_map[128] = {
    0, KEY_ESC, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', KEY_BACKSPACE, KEY_TAB,
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', KEY_ENTER, KEY_CTRL, 'a', 's',
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', KEY_LSHIFT, '\\', 'z', 'x', 'c', 'v',
    'b', 'n', 'm', ',', '.', '/', KEY_RSHIFT, '*', KEY_ALT, KEY_SPACE, KEY_CAPS, KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5,
    KEY_F6, KEY_F7, KEY_F8, KEY_F9, KEY_F10, 0, 0, KEY_HOME, KEY_UP, KEY_PGUP, '-', KEY_LEFT, 0, KEY_RIGHT, '+', KEY_END,
    KEY_DOWN, KEY_PGDOWN, KEY_INSERT, KEY_DELETE, 0, 0, 0, KEY_F11, KEY_F12
};

static const uint8_t extended_scancode_map[128] = {
    [0x48] = KEY_UP,
    [0x50] = KEY_DOWN,
    [0x4B] = KEY_LEFT,
    [0x4D] = KEY_RIGHT,
    [0x47] = KEY_HOME,
    [0x4F] = KEY_END,
    [0x49] = KEY_PGUP,
    [0x51] = KEY_PGDOWN,
    [0x52] = KEY_INSERT,
    [0x53] = KEY_DELETE,
    [0x1C] = KEY_ENTER,
    [0x1D] = KEY_CTRL,
    [0x38] = KEY_ALT
};

static const char shift_map[128] = {
    ['1'] = '!', ['2'] = '@', ['3'] = '#', ['4'] = '$', ['5'] = '%',
    ['6'] = '^', ['7'] = '&', ['8'] = '*', ['9'] = '(', ['0'] = ')',
    ['-'] = '_', ['='] = '+', ['['] = '{', [']'] = '}', ['\\'] = '|',
    [';'] = ':', ['\''] = '"', ['`'] = '~', [','] = '<', ['.'] = '>',
    ['/'] = '?'
};

struct interrupt_registers {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t int_no, err_code;
    uint64_t rip, cs, eflags, rsp, ss;
};

void init_keyboard(void);
void keyboard_handler(struct interrupt_registers *regs);
void enable_keyboard_irq(void);

#endif // KEYBOARD_H