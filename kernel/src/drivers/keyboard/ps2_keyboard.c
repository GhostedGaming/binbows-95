#include <ps2_keyboard.h>
#include <serial.h>
#include <io.h>
#include <interrupts.h>
#include <shell.h>
#include <framebuffer.h>
#include <util.h>

static modifier_state_t modifiers = {0};

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

void init_keyboard(void) {
    modifiers = (modifier_state_t){0};
    
    while (inb(KB_STATUS_PORT) & KB_STATUS_OUTPUT_FULL) {
        inb(KB_DATA_PORT);
    }
    
    while (inb(KB_STATUS_PORT) & KB_STATUS_INPUT_FULL);
    outb(KB_COMMAND_PORT, KB_ENABLE_KEYBOARD);
    while (inb(KB_STATUS_PORT) & KB_STATUS_INPUT_FULL);
    
    enable_keyboard_irq();
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
    
    if (scancode >= sizeof(scancode_to_key)) {
        modifiers.extended = false;
        return;
    }
    
    uint8_t key = scancode_to_key[scancode];
    
    if (key_pressed) {
        received_key = key;
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