#include <serial.h>
#include <mem.h>
#include <framebuffer.h>
#include <util.h>
#include <shell.h>

// Define the constants
const int line_height = 16;
const int char_width = 8;
const uint32_t text_color = 0xFFFFFFFF;
const uint32_t bg_color = 0xFF000000;
const uint32_t cursor_color = 0xFF00FF00;
const uint32_t error_color = 0xFFFF0000;
const uint32_t success_color = 0xFF00FF00;

// Define the shell state
shell_state_t shell_state = {0};

// Forward declarations for static functions
static void shell_print_welcome(void);
static void shell_insert_char(char ch);
static void shell_remove_chars(int start, int count);
static void shell_add_to_history(const char *command);

void shell_init(void) {
    memset(&shell_state, 0, sizeof(shell_state));
    clear_screen();
    move_cursor_to(0, 0);
    shell_print_welcome();
    shell_print_prompt();
}

static void shell_print_welcome(void) {
    shell_print("Binbows v1.2\nType 'help' for available commands.\n\n");
}

void shell_print_prompt(void) {
    draw_text(get_cursor_x(), get_cursor_y(), "$ ", text_color, false);
    move_cursor_right(2 * char_width);
}

void shell_redraw_input(void) {
    int prompt_x = get_cursor_x() - (2 * char_width);
    int prompt_y = get_cursor_y();
    
    // Clear and redraw line in one pass
    draw_rect(prompt_x, prompt_y, get_screen_width() - prompt_x, line_height, bg_color);
    draw_text(prompt_x, prompt_y, "$ ", text_color, false);
    
    int text_x = prompt_x + (2 * char_width);
    draw_text(text_x, prompt_y, shell_state.input_buffer, text_color, false);
    
    // Draw cursor efficiently
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

void shell_draw_input_line(void) {
    shell_redraw_input();
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
    
    // Move screen content up by one line
    for (int y = 0; y < height - line_height; y++) {
        memmove(&fb[y * pitch], &fb[(y + line_height) * pitch], width * sizeof(uint32_t));
    }
    
    // Clear bottom lines
    for (int y = height - line_height; y < height; y++) {
        for (int x = 0; x < width; x++) {
            fb[y * pitch + x] = bg_color;
        }
    }
    
    move_cursor_up(line_height);
}

void shell_print_color(const char *text, uint32_t color) {
    if (!text) return;
    
    const char *p = text;
    while (*p) {
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
        p++;
    }
}

void shell_print(const char *text) {
    shell_print_color(text, text_color);
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

static void shell_insert_char(char ch) {
    if (shell_state.input_index >= MAX_INPUT_LENGTH - 1) return;
    
    // Shift characters right
    memmove(&shell_state.input_buffer[shell_state.cursor_pos + 1],
            &shell_state.input_buffer[shell_state.cursor_pos],
            shell_state.input_index - shell_state.cursor_pos);
    
    shell_state.input_buffer[shell_state.cursor_pos] = ch;
    shell_state.cursor_pos++;
    shell_state.input_index++;
    shell_state.input_buffer[shell_state.input_index] = '\0';
}

static void shell_remove_chars(int start, int count) {
    if (start < 0 || start >= shell_state.input_index || count <= 0) return;
    
    int actual_count = (start + count > shell_state.input_index) ? 
                       shell_state.input_index - start : count;
    
    memmove(&shell_state.input_buffer[start],
            &shell_state.input_buffer[start + actual_count],
            shell_state.input_index - start - actual_count + 1);
    
    shell_state.input_index -= actual_count;
    if (shell_state.cursor_pos > start) {
        shell_state.cursor_pos = (shell_state.cursor_pos > start + actual_count) ?
                                 shell_state.cursor_pos - actual_count : start;
    }
}

void shell_backspace(void) {
    if (shell_state.cursor_pos > 0) {
        shell_remove_chars(shell_state.cursor_pos - 1, 1);
        shell_redraw_input();
    }
}

void shell_delete(void) {
    if (shell_state.cursor_pos < shell_state.input_index) {
        shell_remove_chars(shell_state.cursor_pos, 1);
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

static void shell_add_to_history(const char *command) {
    if (!strlen(command)) return;
    
    // Check if same as previous command
    int prev_index = (shell_state.history_index - 1 + MAX_HISTORY_ENTRIES) % MAX_HISTORY_ENTRIES;
    if (strlen(shell_state.history[prev_index]) > 0 && 
        strcmp(shell_state.history[prev_index], command) == 0) {
        shell_state.history_current = shell_state.history_index;
        return;
    }
    
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

char *input(char received) {
    switch (received) {
        case '\n':
        case '\r':
            shell_state.input_buffer[shell_state.input_index] = '\0';
            strncpy(shell_state.command_buffer, shell_state.input_buffer, sizeof(shell_state.command_buffer) - 1);
            shell_state.command_buffer[sizeof(shell_state.command_buffer) - 1] = '\0';
            if (strlen(shell_state.command_buffer) > 0) {
                shell_add_to_history(shell_state.command_buffer);
            }
            shell_newline();
            shell_clear_input();
            return shell_state.command_buffer;
            
        case '\b':
        case 127:
            shell_backspace();
            return NULL;
            
        case 3:  // Ctrl+C
            shell_cancel_input();
            return NULL;
            
        case 4:  // Ctrl+D
            shell_delete();
            return NULL;
            
        case 1:  // Ctrl+A
            shell_move_cursor_home();
            return NULL;
            
        case 5:  // Ctrl+E
            shell_move_cursor_end();
            return NULL;
            
        case 12:  // Ctrl+L
            clear_screen();
            move_cursor_to(0, 0);
            shell_print_prompt();
            shell_redraw_input();
            return NULL;
            
        case 11:  // Ctrl+K
            shell_state.input_buffer[shell_state.cursor_pos] = '\0';
            shell_state.input_index = shell_state.cursor_pos;
            shell_redraw_input();
            return NULL;
            
        case 21:  // Ctrl+U
            shell_clear_input();
            return NULL;
            
        case 23: { // Ctrl+W
            if (shell_state.cursor_pos > 0) {
                int start = shell_state.cursor_pos - 1;
                while (start > 0 && shell_state.input_buffer[start] == ' ') start--;
                while (start > 0 && shell_state.input_buffer[start] != ' ') start--;
                if (shell_state.input_buffer[start] == ' ') start++;
                
                int chars_to_remove = shell_state.cursor_pos - start;
                shell_remove_chars(start, chars_to_remove);
                shell_redraw_input();
            }
            return NULL;
        }
            
        case 27:  // Escape
            return NULL;
            
        default:
            if (received >= 32 && received <= 126) {
                shell_insert_char(received);
                shell_redraw_input();
            }
            return NULL;
    }
}