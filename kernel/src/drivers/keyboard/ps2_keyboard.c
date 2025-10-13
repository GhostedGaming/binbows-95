#include <ps2_keyboard.h>
#include <serial.h>
#include <io.h>
#include <interrupts.h>
#include <shell.h>
#include <framebuffer.h>
#include <util.h>

static modifier_state_t modifiers = {0};

uint32_t keyboard_process_pid = 0;
bool keyboard_enabled = false;

static const uint8_t scancode_to_key[128] = {
    0, KEY_ESC, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', 
    KEY_BACKSPACE, KEY_TAB, 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', 
    '[', ']', KEY_ENTER, KEY_CTRL, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 
    'l', ';', '\'', '`', KEY_LSHIFT, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', 
    ',', '.', '/', KEY_RSHIFT, '*', KEY_ALT, KEY_SPACE, KEY_CAPS, KEY_F1, 
    KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6, KEY_F7, KEY_F8, KEY_F9, KEY_F10,
    0, 0, KEY_HOME, KEY_UP, KEY_PGUP, 0, KEY_LEFT, 0, KEY_RIGHT, 0, KEY_END,
    KEY_DOWN, KEY_PGDOWN, KEY_INSERT, KEY_DELETE
};

static const char shift_map[] = "!@#$%^&*()_+{}|:\"~<>?";
static const char normal_map[] = "1234567890-=[]\\;'`,./";

#define KBD_BUF_SIZE 256
static char kbd_buf[KBD_BUF_SIZE];
static size_t kbd_head = 0;
static size_t kbd_tail = 0;

void init_keyboard(void) {
    modifiers = (modifier_state_t){0};
    
    while (inb(KB_STATUS_PORT) & KB_STATUS_OUTPUT_FULL) {
        inb(KB_DATA_PORT);
    }
    
    outb(KB_COMMAND_PORT, KB_ENABLE_KEYBOARD);
    
    while (inb(KB_STATUS_PORT) & KB_STATUS_INPUT_FULL);
    
    serial_printf("[KBD] Controller enabled, attempting to enable scanning\n");

    const int max_retries = 10;
    int attempt;
    bool got_ack = false;
    
    for (attempt = 0; attempt < max_retries; attempt++) {
        int timeout = 100000;
        while ((inb(KB_STATUS_PORT) & KB_STATUS_INPUT_FULL) && --timeout > 0);
        
        if (timeout <= 0) {
            serial_printf("[KBD] timeout waiting input empty (attempt %d)\n", attempt);
            for (volatile int i = 0; i < 10000; i++);
            continue;
        }

        outb(KB_DATA_PORT, 0xF4);
        
        timeout = 100000;
        while (!(inb(KB_STATUS_PORT) & KB_STATUS_OUTPUT_FULL) && --timeout > 0);
        
        if (timeout <= 0) {
            serial_printf("[KBD] timeout waiting for ACK (attempt %d)\n", attempt);
            for (volatile int i = 0; i < 10000; i++);
            continue;
        }

        uint8_t resp = inb(KB_DATA_PORT);
        serial_printf("[KBD] response 0x%02x to enable-scanning\n", resp);
        
        if (resp == 0xFA) {
            got_ack = true;
            break;
        } else if (resp == 0xFE) {
            serial_printf("[KBD] resend requested\n");
            continue;
        }
    }

    if (got_ack) {
        enable_keyboard_irq();
        keyboard_enabled = true;
        serial_printf("[KBD] Keyboard enabled (ACK received)\n");
    } else {
        serial_printf("[KBD] Failed to enable keyboard after %d attempts\n", max_retries);
        keyboard_enabled = false;
    }
}

char get_character(uint8_t key) {
    if (key == 0 || key >= 128) return 0;
    
    char c = (char)key;
    
    if (c >= 'a' && c <= 'z') {
        if (modifiers.caps_lock ^ modifiers.shift) {
            c = c - 'a' + 'A';
        }
        return c;
    }
    
    if (modifiers.shift) {
        for (int i = 0; normal_map[i]; i++) {
            if (normal_map[i] == c) {
                return shift_map[i];
            }
        }
    }
    
    return c;
}

void handle_key_press(uint8_t key) {
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
        return;
    }
    
    if (key == KEY_HOME) {
        shell_move_cursor_home();
        return;
    }
    
    if (key == KEY_END) {
        shell_move_cursor_end();
        return;
    }
    
    if (key == KEY_UP) {
        shell_history_up();
        return;
    }
    
    if (key == KEY_DOWN) {
        shell_history_down();
        return;
    }
    
    if (key == KEY_LEFT) {
        shell_move_cursor_left();
        return;
    }
    
    if (key == KEY_RIGHT) {
        shell_move_cursor_right();
        return;
    }
    
    if (key == KEY_DELETE) {
        shell_delete();
        return;
    }
    
    if (key >= 32 && key <= 126) {
        char c = get_character(key);
        if (c == 0) return;
        
        if (modifiers.ctrl) {
            switch (c | 0x20) {
                case 'l': input(12); break;
                case 'u': input(21); break;
                case 'd': input(4); break;
                case 'a': shell_move_cursor_home(); break;
                case 'e': shell_move_cursor_end(); break;
                case 'c': shell_cancel_input(); break;
            }
        } else {
            char *command = input(c);
            if (command) parse_command();
        }
    }
}

void handle_key_release(uint8_t key) {
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
    
    if (scancode >= (sizeof(scancode_to_key) / sizeof(scancode_to_key[0]))) {
        modifiers.extended = false;
        return;
    }
    
    uint8_t key = scancode_to_key[scancode];
    
    if (key_pressed) {
        handle_key_press(key);
    } else {
        handle_key_release(key);
    }
    
    modifiers.extended = false;
}

void enable_keyboard_irq(void) {
    uint8_t mask = inb(PIC1_DATA_PORT);
    mask &= ~(1 << 1);
    outb(PIC1_DATA_PORT, mask);
}

void disable_keyboard_irq(void) {
    uint8_t mask = inb(PIC1_DATA_PORT);
    mask |= (1 << 1);
    outb(PIC1_DATA_PORT, mask);
    keyboard_enabled = false;
    kbd_tail = kbd_head;
    received_key = 0;
}

void kbd_enqueue_char(char c) {
    size_t next = (kbd_head + 1) % KBD_BUF_SIZE;
    if (next == kbd_tail) {
        return;
    }
    kbd_buf[kbd_head] = c;
    kbd_head = next;
}

size_t kbd_available(void) {
    if (kbd_head >= kbd_tail) return kbd_head - kbd_tail;
    return KBD_BUF_SIZE - (kbd_tail - kbd_head);
}

size_t kbd_read_chars(char *buf, size_t len) {
    size_t i = 0;
    while (i < len && kbd_tail != kbd_head) {
        buf[i++] = kbd_buf[kbd_tail];
        kbd_tail = (kbd_tail + 1) % KBD_BUF_SIZE;
    }
    return i;
}

void kbd_clear_buffer(void) {
    kbd_tail = kbd_head;
}