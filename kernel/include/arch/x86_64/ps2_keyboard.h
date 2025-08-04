#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

// Hardware constants
#define KB_DATA_PORT    0x60
#define KB_STATUS_PORT  0x64
#define KB_COMMAND_PORT 0x64
#define KB_STATUS_OUTPUT_FULL   0x01
#define KB_STATUS_INPUT_FULL    0x02
#define KB_ENABLE_KEYBOARD      0xAE
#define PIC1_DATA_PORT  0x21

// Key definitions - optimized enum with better grouping
typedef enum {
    KEY_UNKNOWN = 0,
    
    // Control keys (1-31 for easy bit manipulation)
    KEY_ESC = 1,
    KEY_CTRL = 2,
    KEY_LSHIFT = 3,
    KEY_RSHIFT = 4,
    KEY_ALT = 5,
    KEY_CAPS = 6,
    
    // Special ASCII keys
    KEY_BACKSPACE = 8,   // '\b'
    KEY_TAB = 9,         // '\t'
    KEY_ENTER = 10,      // '\n'
    KEY_SPACE = 32,      // ' '
    
    // Extended keys (128+)
    KEY_F1 = 128, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6,
    KEY_F7, KEY_F8, KEY_F9, KEY_F10, KEY_F11, KEY_F12,
    KEY_HOME, KEY_END, KEY_PGUP, KEY_PGDOWN,
    KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT,
    KEY_INSERT, KEY_DELETE
} special_key_t;

// Packed modifier state for better cache performance
typedef struct {
    uint8_t caps_lock : 1;
    uint8_t shift : 1;
    uint8_t ctrl : 1;
    uint8_t alt : 1;
    uint8_t extended : 1;
    uint8_t reserved : 3;
} __attribute__((packed)) modifier_state_t;

static modifier_state_t modifiers = {0};

// Optimized scancode mapping - removed gaps and used designated initializers
static const uint8_t scancode_map[89] = {
    [0] = 0, [1] = KEY_ESC, [2] = '1', [3] = '2', [4] = '3', [5] = '4',
    [6] = '5', [7] = '6', [8] = '7', [9] = '8', [10] = '9', [11] = '0',
    [12] = '-', [13] = '=', [14] = KEY_BACKSPACE, [15] = KEY_TAB,
    [16] = 'q', [17] = 'w', [18] = 'e', [19] = 'r', [20] = 't', [21] = 'y',
    [22] = 'u', [23] = 'i', [24] = 'o', [25] = 'p', [26] = '[', [27] = ']',
    [28] = KEY_ENTER, [29] = KEY_CTRL, [30] = 'a', [31] = 's', [32] = 'd',
    [33] = 'f', [34] = 'g', [35] = 'h', [36] = 'j', [37] = 'k', [38] = 'l',
    [39] = ';', [40] = '\'', [41] = '`', [42] = KEY_LSHIFT, [43] = '\\',
    [44] = 'z', [45] = 'x', [46] = 'c', [47] = 'v', [48] = 'b', [49] = 'n',
    [50] = 'm', [51] = ',', [52] = '.', [53] = '/', [54] = KEY_RSHIFT,
    [55] = '*', [56] = KEY_ALT, [57] = KEY_SPACE, [58] = KEY_CAPS,
    [59] = KEY_F1, [60] = KEY_F2, [61] = KEY_F3, [62] = KEY_F4, [63] = KEY_F5,
    [64] = KEY_F6, [65] = KEY_F7, [66] = KEY_F8, [67] = KEY_F9, [68] = KEY_F10,
    [71] = KEY_HOME, [72] = KEY_UP, [73] = KEY_PGUP, [75] = KEY_LEFT,
    [77] = KEY_RIGHT, [79] = KEY_END, [80] = KEY_DOWN, [81] = KEY_PGDOWN,
    [82] = KEY_INSERT, [83] = KEY_DELETE, [87] = KEY_F11, [88] = KEY_F12
};

// Compact extended scancode mapping
static const struct {
    uint8_t scancode;
    uint8_t key;
} extended_map[] = {
    {0x1C, KEY_ENTER}, {0x1D, KEY_CTRL}, {0x38, KEY_ALT},
    {0x47, KEY_HOME}, {0x48, KEY_UP}, {0x49, KEY_PGUP},
    {0x4B, KEY_LEFT}, {0x4D, KEY_RIGHT}, {0x4F, KEY_END},
    {0x50, KEY_DOWN}, {0x51, KEY_PGDOWN}, {0x52, KEY_INSERT}, {0x53, KEY_DELETE}
};

// Optimized shift mapping using lookup table
static const char shift_chars[] = "!@#$%^&*()_+{}|:\"~<>?";
static const char normal_chars[] = "1234567890-=[]\\;'`,./";

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