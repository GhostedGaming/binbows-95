#include <shell.h>
#include <serial.h>
#include <mem.h>
#include <framebuffer.h>
#include <ps2_keyboard.h>
#include <util.h>
#include <stdarg.h>

#define line_height 16
#define char_width 8
#define text_color rgb_to_color(255, 255, 255)
#define bg_color rgb_to_color(0, 0, 0)
#define cursor_color rgb_to_color(255, 255, 255)
#define error_color rgb_to_color(255, 0, 0)
#define success_color rgb_to_color(0, 255, 0)

shell_state_t shell_state = {0};
volatile uint8_t received_key = 0;

void shell_init(void) {
    memset(&shell_state, 0, sizeof(shell_state));
    clear_screen();
    move_cursor_to(0, 0);
    shell_print("Binbows v1.2\nType 'help' for available commands.\n\n");
    shell_print_prompt();
}

void shell_print_prompt(void) {
    draw_text(get_cursor_x(), get_cursor_y(), "$ ", text_color, false);
    move_cursor_right(2 * char_width);
}

void shell_redraw_input(void) {
    int prompt_x = get_cursor_x() - (2 * char_width);
    int prompt_y = get_cursor_y();
    
    draw_rect(prompt_x, prompt_y, get_screen_width() - prompt_x, line_height, bg_color);
    draw_text(prompt_x, prompt_y, "$ ", text_color, false);
    
    int text_x = prompt_x + (2 * char_width);
    draw_text(text_x, prompt_y, shell_state.input_buffer, text_color, false);
    
    if (shell_state.cursor_visible) {
        int cursor_x = text_x + (shell_state.cursor_pos * char_width);
        draw_rect(cursor_x, prompt_y, char_width, line_height, cursor_color);
        if (shell_state.cursor_pos < shell_state.input_index && 
            shell_state.input_buffer[shell_state.cursor_pos]) {
            char cursor_char[2] = {shell_state.input_buffer[shell_state.cursor_pos], '\0'};
            draw_text(cursor_x, prompt_y, cursor_char, bg_color, false);
        }
    }
}

void shell_update_cursor(void) {
    if (++shell_state.cursor_blink_counter >= CURSOR_BLINK_RATE) {
        shell_state.cursor_visible = !shell_state.cursor_visible;
        shell_state.cursor_blink_counter = 0;
        shell_redraw_input();
    }
}

void shell_newline(void) {
    move_cursor_to(0, get_cursor_y() + line_height);
    if (get_cursor_y() >= get_screen_height() - line_height) {
        shell_scroll_up();
    }
}

void shell_scroll_up(void) {
    uint32_t *fb = (uint32_t*)fb_ptr;
    int width = get_screen_width();
    int height = get_screen_height();
    int pitch = fb_pitch;
    
    for (int y = 0; y < height - line_height; y++) {
        memmove(&fb[y * pitch], &fb[(y + line_height) * pitch], width * sizeof(uint32_t));
    }
    
    for (int y = height - line_height; y < height; y++) {
        for (int x = 0; x < width; x++) {
            fb[y * pitch + x] = bg_color;
        }
    }
    
    move_cursor_up(line_height);
}

void shell_print_color(const char *text, uint32_t color) {
    if (!text) return;
    
    for (const char *p = text; *p; p++) {
        switch (*p) {
            case '\n':
                shell_newline();
                break;
            case '\r':
                move_cursor_to(0, get_cursor_y());
                break;
            case '\t': {
                int spaces = TAB_SIZE - (get_cursor_x() / char_width) % TAB_SIZE;
                for (int i = 0; i < spaces; i++) {
                    draw_char(get_cursor_x(), get_cursor_y(), ' ', color);
                    move_cursor_right(char_width);
                    if (get_cursor_x() >= get_screen_width() - char_width) {
                        shell_newline();
                        break;
                    }
                }
                break;
            }
            default:
                draw_char(get_cursor_x(), get_cursor_y(), *p, color);
                move_cursor_right(char_width);
                if (get_cursor_x() >= get_screen_width() - char_width) {
                    shell_newline();
                }
                break;
        }
    }
}

void shell_print(const char *text) {
    shell_print_color(text, text_color);
}

void shell_printf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    
    while (*format) {
        if (*format == '%' && *(format + 1)) {
            format++;
            
            switch (*format) {
                case 'd': {
                    int val = va_arg(args, int);
                    char buffer[32];
                    int len = 0;
                    int temp = val;
                    
                    if (val < 0) {
                        shell_print("-");
                        val = -val;
                    }
                    
                    if (val == 0) {
                        shell_print("0");
                    } else {
                        while (temp > 0) {
                            buffer[len++] = '0' + (temp % 10);
                            temp /= 10;
                        }
                        for (int i = len - 1; i >= 0; i--) {
                            char c[2] = {buffer[i], '\0'};
                            shell_print(c);
                        }
                    }
                    break;
                }
                case 's': {
                    char *str = va_arg(args, char *);
                    if (str) shell_print(str);
                    else shell_print("(null)");
                    break;
                }
                case 'c': {
                    char c = (char)va_arg(args, int);
                    char temp[2] = {c, '\0'};
                    shell_print(temp);
                    break;
                }
                case '%':
                    shell_print("%");
                    break;
                default: {
                    char temp[2] = {*format, '\0'};
                    shell_print(temp);
                    break;
                }
            }
        } else {
            char temp[2] = {*format, '\0'};
            shell_print(temp);
        }
        format++;
    }
    
    va_end(args);
}

void shell_error(const char *text) {
    shell_print_color("Error: ", error_color);
    shell_print(text);
    shell_print("\n");
}

void shell_success(const char *text) {
    shell_print_color(text, success_color);
    shell_print("\n");
}

void insert_character(char ch) {
    if (shell_state.input_index >= MAX_INPUT_LENGTH - 1) return;
    
    memmove(&shell_state.input_buffer[shell_state.cursor_pos + 1],
            &shell_state.input_buffer[shell_state.cursor_pos],
            shell_state.input_index - shell_state.cursor_pos);
    
    shell_state.input_buffer[shell_state.cursor_pos] = ch;
    shell_state.cursor_pos++;
    shell_state.input_index++;
    shell_state.input_buffer[shell_state.input_index] = '\0';
}

void remove_character(int pos) {
    if (pos < 0 || pos >= shell_state.input_index) return;
    
    memmove(&shell_state.input_buffer[pos],
            &shell_state.input_buffer[pos + 1],
            shell_state.input_index - pos);
    
    shell_state.input_index--;
    if (shell_state.cursor_pos > pos) {
        shell_state.cursor_pos--;
    }
}

void shell_backspace(void) {
    if (shell_state.cursor_pos > 0) {
        remove_character(shell_state.cursor_pos - 1);
        shell_redraw_input();
    }
}

void shell_delete(void) {
    if (shell_state.cursor_pos < shell_state.input_index) {
        remove_character(shell_state.cursor_pos);
        shell_redraw_input();
    }
}

void shell_move_cursor_left(void) {
    if (shell_state.cursor_pos > 0) {
        shell_state.cursor_pos--;
        shell_redraw_input();
    }
}

void shell_move_cursor_right(void) {
    if (shell_state.cursor_pos < shell_state.input_index) {
        shell_state.cursor_pos++;
        shell_redraw_input();
    }
}

void shell_move_cursor_home(void) {
    shell_state.cursor_pos = 0;
    shell_redraw_input();
}

void shell_move_cursor_end(void) {
    shell_state.cursor_pos = shell_state.input_index;
    shell_redraw_input();
}

void shell_clear_input(void) {
    shell_state.input_index = shell_state.cursor_pos = 0;
    shell_state.input_buffer[0] = '\0';
    shell_redraw_input();
}

void add_to_history(const char *command) {
    if (!strlen(command)) return;
    
    strcpy(shell_state.history[shell_state.history_index], command);
    shell_state.history_index = (shell_state.history_index + 1) % MAX_HISTORY_ENTRIES;
    shell_state.history_current = shell_state.history_index;
}

void shell_history_up(void) {
    int prev = (shell_state.history_current - 1 + MAX_HISTORY_ENTRIES) % MAX_HISTORY_ENTRIES;
    if (strlen(shell_state.history[prev]) > 0) {
        shell_state.history_current = prev;
        strcpy(shell_state.input_buffer, shell_state.history[shell_state.history_current]);
        shell_state.input_index = shell_state.cursor_pos = strlen(shell_state.input_buffer);
        shell_redraw_input();
    }
}

void shell_history_down(void) {
    int next = (shell_state.history_current + 1) % MAX_HISTORY_ENTRIES;
    if (next != shell_state.history_index) {
        shell_state.history_current = next;
        strcpy(shell_state.input_buffer, shell_state.history[shell_state.history_current]);
        shell_state.input_index = shell_state.cursor_pos = strlen(shell_state.input_buffer);
        shell_redraw_input();
    } else {
        shell_clear_input();
        shell_state.history_current = shell_state.history_index;
    }
}

void shell_cancel_input(void) {
    shell_clear_input();
    shell_newline();
    shell_print("^C\n");
    shell_print_prompt();
    shell_redraw_input();
}

char wait_for_input(void) {
    received_key = 0;
    
    while (!received_key) {
        keyboard_handler(NULL);
        asm volatile ("nop");
    }
    
    char result = get_character(received_key);
    shell_backspace();
    return (char)received_key;
}

char *input(char received) {
    switch (received) {
        case '\n':
        case '\r':
            shell_state.input_buffer[shell_state.input_index] = '\0';
            strcpy(shell_state.command_buffer, shell_state.input_buffer);
            if (strlen(shell_state.command_buffer) > 0) {
                add_to_history(shell_state.command_buffer);
            }
            shell_newline();
            shell_clear_input();
            return shell_state.command_buffer;
            
        case '\b':
        case 127:
            shell_backspace();
            return NULL;
            
        case 3:
            shell_cancel_input();
            return NULL;
            
        case 4:
            shell_delete();
            return NULL;
            
        case 1:
            shell_move_cursor_home();
            return NULL;
            
        case 5:
            shell_move_cursor_end();
            return NULL;
            
        case 12:
            clear_screen();
            move_cursor_to(0, 0);
            shell_print_prompt();
            shell_redraw_input();
            return NULL;
            
        case 21:
            shell_clear_input();
            return NULL;
            
        case 27:
            return NULL;
            
        default:
            if (received >= 32 && received <= 126) {
                insert_character(received);
                shell_redraw_input();
            }
            return NULL;
    }
}