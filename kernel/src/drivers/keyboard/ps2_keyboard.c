#include <ps2_keyboard.h>
#include <serial.h>
#include <io.h>
#include <interrupts.h>
#include <shell.h>
#include <framebuffer.h>
#include <util.h>

// Inline functions for better performance
static inline void keyboard_wait_input(void) {
    while (inb(KB_STATUS_PORT) & KB_STATUS_INPUT_FULL);
}

static inline bool is_printable(uint8_t key) {
    return (key >= 32 && key <= 126);
}

static inline bool is_letter(char c) {
    return (c >= 'a' && c <= 'z');
}

static char get_character(uint8_t key) {
    if (key == 0 || key >= 128) return 0;
    
    char c = (char)key;
    
    if (is_letter(c)) {
        if (modifiers.caps_lock ^ modifiers.shift) {
            c ^= 0x20;
        }
        return c;
    }
    
    if (modifiers.shift) {
        const char* pos = strchr(normal_chars, c);
        if (pos) {
            return shift_chars[pos - normal_chars];
        }
    }
    
    return c;
}

static uint8_t get_extended_key(uint8_t scancode) {
    for (long unsigned int i = 0; i < sizeof(extended_map) / sizeof(extended_map[0]); i++) {
        if (extended_map[i].scancode == scancode) {
            return extended_map[i].key;
        }
    }
    return 0;
}

typedef void (*key_handler_func_t)(uint8_t key);

static void handle_ctrl_combination(char c) {
    switch (c | 0x20) {
        case 'l': input(12); break;
        case 'k': input(11); break;
        case 'u': input(21); break;
        case 'w': input(23); break;
        case 'd': input(4); break; 
        case 'a': shell_move_cursor_home(); break;
        case 'e': shell_move_cursor_end(); break;
        case 'c': shell_cancel_input(); break;
    }
}

static void handle_navigation_key(uint8_t key) {
    switch (key) {
        case KEY_HOME: shell_move_cursor_home(); break;
        case KEY_END: shell_move_cursor_end(); break;
        case KEY_UP: shell_history_up(); break;
        case KEY_DOWN: shell_history_down(); break;
        case KEY_LEFT: shell_move_cursor_left(); break;
        case KEY_RIGHT: shell_move_cursor_right(); break;
        case KEY_DELETE: shell_delete(); break;
    }
}

static void process_key_input(uint8_t key, bool pressed) {
    if (!pressed) return;
    
    switch (key) {
        case KEY_LSHIFT:
        case KEY_RSHIFT:
            modifiers.shift = true;
            return;
        case KEY_CTRL:
            modifiers.ctrl = true;
            return;
        case KEY_ALT:
            modifiers.alt = true;
            return;
        case KEY_CAPS:
            modifiers.caps_lock = !modifiers.caps_lock;
            return;
    }
    
    if (key == KEY_BACKSPACE) {
        char *result = input('\b');
        if (result) parse_command();
        return;
    }
    
    if (key == KEY_ENTER) {
        char *command = input('\n');
        if (command) parse_command();
        serial_printf("\r\n");
        return;
    }
    
    if (key >= KEY_HOME && key <= KEY_DELETE) {
        handle_navigation_key(key);
        return;
    }
    
    if (is_printable(key)) {
        char c = get_character(key);
        if (c == 0) return;
        
        if (modifiers.ctrl) {
            handle_ctrl_combination(c);
        } else if (modifiers.alt) {
            serial_printf("[ALT+%c]\r\n", c);
        } else {
            char *command = input(c);
            if (command) parse_command();
            serial_printf("%c", c);
        }
    }
}

void init_keyboard(void) {
    modifiers = (modifier_state_t){0};
    
    while (inb(KB_STATUS_PORT) & KB_STATUS_OUTPUT_FULL) {
        inb(KB_DATA_PORT);
    }
    
    keyboard_wait_input();
    outb(KB_COMMAND_PORT, KB_ENABLE_KEYBOARD);
    keyboard_wait_input();
    
    serial_printf("Keyboard initialized\r\n");
    enable_keyboard_irq();
}

void keyboard_handler(struct interrupt_registers *regs) {
    (void)regs;
    
    if (!(inb(KB_STATUS_PORT) & KB_STATUS_OUTPUT_FULL)) {
        return;
    }
    
    uint8_t scancode_raw = inb(KB_DATA_PORT);
    
    if (scancode_raw == 0xE0) {
        modifiers.extended = true;
        return;
    }
    
    uint8_t scancode = scancode_raw & 0x7F;
    bool key_pressed = !(scancode_raw & 0x80);
    
    if (scancode >= sizeof(scancode_map)) {
        modifiers.extended = false;
        return;
    }
    
    uint8_t key;
    if (modifiers.extended) {
        key = get_extended_key(scancode);
        modifiers.extended = false;
    } else {
        key = scancode_map[scancode];
    }
    
    if (key == 0) return;
    
    if (!key_pressed) {
        switch (key) {
            case KEY_LSHIFT:
            case KEY_RSHIFT:
                modifiers.shift = false;
                break;
            case KEY_CTRL:
                modifiers.ctrl = false;
                break;
            case KEY_ALT:
                modifiers.alt = false;
                break;
        }
        return;
    }
    
    process_key_input(key, key_pressed);
}

bool is_shift_pressed(void) {
    return modifiers.shift;
}

bool is_ctrl_pressed(void) {
    return modifiers.ctrl;
}

bool is_alt_pressed(void) {
    return modifiers.alt;
}

bool is_caps_lock_on(void) {
    return modifiers.caps_lock;
}

void enable_keyboard_irq(void) {
    uint8_t mask = inb(PIC1_DATA_PORT);
    mask &= ~(1 << 1);
    outb(PIC1_DATA_PORT, mask);
    serial_printf("Keyboard: IRQ1 enabled in PIC\r\n");
}