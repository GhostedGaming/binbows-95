#include <ps2_keyboard.h>
#include <serial.h>
#include <io.h>
#include <idt.h>

static void keyboard_wait_input(void) {
    while (inb(KB_STATUS_PORT) & KB_STATUS_INPUT_FULL);
}

static char get_character(uint8_t key) {
    if (key == 0 || key >= 128) return 0;
    
    char c = (char)key;
    
    if (c >= 'a' && c <= 'z') {
        bool should_uppercase = caps_lock_on ^ shift_pressed;
        if (should_uppercase) {
            c = c - 'a' + 'A';
        }
    } else if (shift_pressed && shift_map[(unsigned char)c]) {
        c = shift_map[(unsigned char)c];                        
    }
    
    return c;
}

static void handle_special_key(uint8_t key, bool pressed) {
    if (!pressed) return;
    
    switch (key) {
        case KEY_ESC:
            serial_printf("[ESC]\r\n");
            break;
            
        case KEY_F1: case KEY_F2: case KEY_F3: case KEY_F4: case KEY_F5: case KEY_F6:
        case KEY_F7: case KEY_F8: case KEY_F9: case KEY_F10: case KEY_F11: case KEY_F12: {
            int f_num = key - KEY_F1 + 1;
            serial_printf("[F%d]\r\n", f_num);
            break;
        }
            
        case KEY_HOME:
            serial_printf("[HOME]\r\n");
            break;
            
        case KEY_END:
            serial_printf("[END]\r\n");
            break;
            
        case KEY_UP:
            serial_printf("[UP]\r\n");
            break;
            
        case KEY_DOWN:
            serial_printf("[DOWN]\r\n");
            break;
            
        case KEY_LEFT:
            break;
        case KEY_RIGHT:
            break;
            
        case KEY_INSERT:
            serial_printf("[INSERT]\r\n");
            break;
            
        case KEY_DELETE:
            serial_printf("[DELETE]\r\n");
            break;
            
        case KEY_PGUP:
            serial_printf("[PGUP]\r\n");
            break;
            
        case KEY_PGDOWN:
            serial_printf("[PGDOWN]\r\n");
            break;
            
        case KEY_TAB:
            serial_printf("[TAB]\r\n");
            break;
            
        default:
            break;
    }
}

static void handle_modifier_key(uint8_t key, bool pressed) {
    switch (key) {
        case KEY_LSHIFT:
        case KEY_RSHIFT:
            shift_pressed = pressed;
            break;
            
        case KEY_CTRL:
            ctrl_pressed = pressed;
            break;
            
        case KEY_ALT:
            alt_pressed = pressed;
            break;
            
        case KEY_CAPS:
            if (pressed) {
                caps_lock_on = !caps_lock_on;
                serial_printf("[CAPS LOCK %s]\r\n", caps_lock_on ? "ON" : "OFF");
            }
            break;
    }
}

static void handle_printable_key(uint8_t key, bool pressed) {
    if (!pressed) return;
    
    char c = get_character(key);
    if (c == 0) return;
    
    if (ctrl_pressed) {
        switch (c) {
            default:
                break;
        }
    } else if (alt_pressed) {
        serial_printf("[ALT+%c]\r\n", c);
    } else {
        serial_printf("%c", c);
    }
}

void init_keyboard(void) {
    caps_lock_on = false;
    shift_pressed = false;
    ctrl_pressed = false;
    alt_pressed = false;
    extended_scancode = false;
    
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
        extended_scancode = true;
        return;
    }
    
    uint8_t scancode = scancode_raw & 0x7F;
    bool key_pressed = !(scancode_raw & 0x80);
    
    if (scancode >= 128) {
        extended_scancode = false;
        return;
    }
    
    uint8_t key;
    if (extended_scancode) {
        key = extended_scancode_map[scancode];
        extended_scancode = false;
    } else {
        key = scancode_map[scancode];
    }
    
    if (key == 0) return;
    
    if (key == KEY_LSHIFT || key == KEY_RSHIFT || key == KEY_CTRL || key == KEY_ALT || key == KEY_CAPS) {
        handle_modifier_key(key, key_pressed);
    } else if (key == KEY_BACKSPACE) {
        if (key_pressed) {
            serial_printf("[BACKSPACE]\r\n");
        }
    } else if (key == KEY_ENTER) {
        if (key_pressed) {
            serial_printf("\r\n");
        }
    } else if (key >= KEY_F1 && key <= KEY_DELETE) {
        handle_special_key(key, key_pressed);
    } else if (key >= 32 && key <= 126) {
        handle_printable_key(key, key_pressed);
    }
}

// Helpers
bool is_shift_pressed(void) {
    return shift_pressed;
}

bool is_ctrl_pressed(void) {
    return ctrl_pressed;
}

bool is_alt_pressed(void) {
    return alt_pressed;
}

bool is_caps_lock_on(void) {
    return caps_lock_on;
}

void enable_keyboard_irq(void) {
    uint8_t mask = inb(0x21);
    mask &= ~(1 << 1);
    outb(0x21, mask);
    serial_printf("Keyboard: IRQ1 enabled in PIC\r\n");
}