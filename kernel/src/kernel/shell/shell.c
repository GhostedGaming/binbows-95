#include <stdint.h>
#include <shell.h>
#include <framebuffer.h>
#include <stdarg.h>
#include <util.h>
#include <ps2_keyboard.h>
#include <commands.h>

volatile uint8_t received_key = 0;
shell_state_t shell_state;

void shell_init(void) {
    fb_print("Binbows-95 kernel 9.0\n", rgb_to_color(100, 200, 255)); // added newline
    fb_print("Type 'help' for a list of commands.\n\n", rgb_to_color(150, 150, 150));
    memset(&shell_state, 0, sizeof(shell_state_t));
    shell_state.cursor_visible = 1;
}

void shell_print_prompt(void) {
    fb_print("$ ", rgb_to_color(100, 200, 255));
    shell_state.prompt_x = get_cursor_x();
    shell_state.prompt_y = get_cursor_y();
}

void shell_redraw_input(void) {
    int prompt_x = shell_state.prompt_x;
    int prompt_y = shell_state.prompt_y;

    draw_rect(prompt_x, prompt_y, MAX_INPUT_LENGTH * FONT_WIDTH, FONT_HEIGHT, 0x00000000);

    for (int i = 0; i < shell_state.input_index; i++) {
        draw_char(prompt_x + i * FONT_WIDTH, prompt_y, shell_state.input_buffer[i], 0xFFFFFFFF);
    }

    move_cursor_to(prompt_x + shell_state.cursor_pos * FONT_WIDTH, prompt_y);
}

void shell_update_cursor(void) {
    shell_state.cursor_blink_counter++;
    if (shell_state.cursor_blink_counter >= CURSOR_BLINK_RATE) {
        shell_state.cursor_blink_counter = 0;
        shell_state.cursor_visible = !shell_state.cursor_visible;
        
        int cursor_x = get_cursor_x();
        int cursor_y = get_cursor_y();
        
        if (shell_state.cursor_visible) {
            draw_rect(cursor_x, cursor_y + FONT_HEIGHT - 2, FONT_WIDTH, 2, 0xFFFFFFFF);
        } else {
            draw_rect(cursor_x, cursor_y + FONT_HEIGHT - 2, FONT_WIDTH, 2, 0x00000000);
        }
    }
}

void shell_newline(void) {
    fb_printf("\n", 0xFFFFFFFF);
}

void shell_scroll_up(void) {
    if (shell_state.history_index > 0) {
        shell_state.history_index--;
    }
}

void shell_print_color(const char *text, uint32_t color) {
    fb_print(text, color);
    if (text[strlen(text) - 1] != '\n') fb_print("\n", color); // ensure newline
}

void shell_printf(const char *format, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    fb_print(buffer, 0xFFFFFFFF);
    if (buffer[strlen(buffer) - 1] != '\n') fb_print("\n", 0xFFFFFFFF); // ensure newline
}

void shell_print(const char *text) { 
    fb_print(text, 0xFFFFFFFF);
    if (text[strlen(text) - 1] != '\n') fb_print("\n", 0xFFFFFFFF); // ensure newline
}

void shell_error(const char *text) {
    fb_print(text, rgb_to_color(255, 100, 100));
    if (text[strlen(text) - 1] != '\n') fb_print("\n", rgb_to_color(255, 100, 100)); // added newline
}

void shell_success(const char *text) {
    fb_print(text, rgb_to_color(100, 255, 100));
    if (text[strlen(text) - 1] != '\n') fb_print("\n", rgb_to_color(100, 255, 100)); // added newline
}

char *input(char received) {
    if (received == '\n' || received == '\r') {
        shell_state.input_buffer[shell_state.input_index] = '\0';

        if (shell_state.input_index == 0) {
            shell_newline();
            shell_print_prompt();
            shell_redraw_input();
            return NULL;
        }

        strncpy(shell_state.command_buffer, shell_state.input_buffer, MAX_INPUT_LENGTH);
        shell_state.command_buffer[MAX_INPUT_LENGTH - 1] = '\0';

        int hist_pos = shell_state.history_index % MAX_HISTORY_ENTRIES;
        strcpy(shell_state.history[hist_pos], shell_state.command_buffer);
        shell_state.history_index++;
        shell_state.history_current = shell_state.history_index;

        shell_newline();

        shell_state.input_index = 0;
        shell_state.cursor_pos = 0;
        shell_state.input_buffer[0] = '\0';
        
        return shell_state.command_buffer;
    }

    if (received == '\b' || received == 127) {
        shell_backspace();
        return NULL;
    }

    if (shell_state.input_index < MAX_INPUT_LENGTH - 1) {
        for (int i = shell_state.input_index; i > shell_state.cursor_pos; i--) {
            shell_state.input_buffer[i] = shell_state.input_buffer[i - 1];
        }

        shell_state.input_buffer[shell_state.cursor_pos] = received;
        shell_state.input_index++;
        shell_state.cursor_pos++;

        shell_redraw_input();
    }

    return NULL;
}

void shell_backspace(void) {
    if (shell_state.cursor_pos > 0) {
        for (int i = shell_state.cursor_pos - 1; i < shell_state.input_index - 1; i++) {
            shell_state.input_buffer[i] = shell_state.input_buffer[i + 1];
        }
        
        shell_state.input_index--;
        shell_state.cursor_pos--;
        shell_state.input_buffer[shell_state.input_index] = '\0';
        
        shell_redraw_input();
    }
}

void shell_delete(void) {
    if (shell_state.cursor_pos < shell_state.input_index) {
        for (int i = shell_state.cursor_pos; i < shell_state.input_index - 1; i++) {
            shell_state.input_buffer[i] = shell_state.input_buffer[i + 1];
        }
        
        shell_state.input_index--;
        shell_state.input_buffer[shell_state.input_index] = '\0';
        
        shell_redraw_input();
    }
}

void shell_move_cursor_left(void) {
    if (shell_state.cursor_pos > 0) {
        shell_state.cursor_pos--;
        move_cursor_to(shell_state.prompt_x + shell_state.cursor_pos * FONT_WIDTH, shell_state.prompt_y);
    }
}

void shell_move_cursor_right(void) {
    if (shell_state.cursor_pos < shell_state.input_index) {
        shell_state.cursor_pos++;
        move_cursor_to(shell_state.prompt_x + shell_state.cursor_pos * FONT_WIDTH, shell_state.prompt_y);
    }
}

void shell_move_cursor_home(void) {
    shell_state.cursor_pos = 0;
    move_cursor_to(shell_state.prompt_x, shell_state.prompt_y);
}

void shell_move_cursor_end(void) {
    shell_state.cursor_pos = shell_state.input_index;
    move_cursor_to(shell_state.prompt_x + shell_state.cursor_pos * FONT_WIDTH, shell_state.prompt_y);
}

void shell_clear_input(void) {
    memset(shell_state.input_buffer, 0, MAX_INPUT_LENGTH);
    shell_state.input_index = 0;
    shell_state.cursor_pos = 0;
    shell_redraw_input();
}

void shell_cancel_input(void) {
    shell_clear_input();
    shell_newline();
    shell_print_prompt();
}

char wait_for_input(void) {
    while (received_key == 0);
    char key = received_key;
    received_key = 0;
    return key;
}

void shell_history_up(void) {
    if (shell_state.history_current > 0) {
        shell_state.history_current--;
        strcpy(shell_state.input_buffer, shell_state.history[shell_state.history_current % MAX_HISTORY_ENTRIES]);
        shell_state.input_index = strlen(shell_state.input_buffer);
        shell_state.cursor_pos = shell_state.input_index;
        shell_redraw_input();
    }
}

void shell_history_down(void) {
    if (shell_state.history_current < shell_state.history_index - 1) {
        shell_state.history_current++;
        strcpy(shell_state.input_buffer, shell_state.history[shell_state.history_current % MAX_HISTORY_ENTRIES]);
        shell_state.input_index = strlen(shell_state.input_buffer);
        shell_state.cursor_pos = shell_state.input_index;
        shell_redraw_input();
    } else if (shell_state.history_current == shell_state.history_index - 1) {
        shell_state.history_current = shell_state.history_index;
        shell_clear_input();
    }
}

void shell_process(void) {
    while (1) {
        char received = wait_for_input();
        char *cmd = input(received);
        if (cmd) {
            parse_command();
        }
    }
}
