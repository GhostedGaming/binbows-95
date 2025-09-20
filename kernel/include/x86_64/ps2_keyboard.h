#ifndef PS2_KEYBOARD_H
#define PS2_KEYBOARD_H

#include <stdint.h>
#include <stdbool.h>

#define KB_DATA_PORT 0x60
#define KB_STATUS_PORT 0x64
#define KB_COMMAND_PORT 0x64
#define KB_STATUS_OUTPUT_FULL 0x01
#define KB_STATUS_INPUT_FULL 0x02
#define KB_ENABLE_KEYBOARD 0xAE
#define PIC1_DATA_PORT 0x21

typedef enum {
    KEY_UNKNOWN = 0,
    KEY_ESC = 1,
    KEY_CTRL = 2,
    KEY_LSHIFT = 3,
    KEY_RSHIFT = 4,
    KEY_ALT = 5,
    KEY_CAPS = 6,
    KEY_BACKSPACE = 8,
    KEY_TAB = 9,
    KEY_ENTER = 10,
    KEY_SPACE = 32,
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
    KEY_DELETE = 149
} key_t;

typedef struct {
    bool caps_lock;
    bool shift;
    bool ctrl;
    bool alt;
    bool extended;
} modifier_state_t;

struct interrupt_registers {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t int_no, err_code;
    uint64_t rip, cs, eflags, rsp, ss;
};

void init_keyboard(void);
void keyboard_handler(struct interrupt_registers *regs);
void enable_keyboard_irq(void);
char get_character(uint8_t key);

#endif