#ifndef SHELL_H
#define SHELL_H

#include <stdint.h>

#define MAX_INPUT_LENGTH 256
#define MAX_HISTORY_ENTRIES 10
#define MAX_ARGS 8
#define MAX_ARG_LENGTH 32
#define CURSOR_BLINK_RATE 30
#define TAB_SIZE 4

typedef struct {
    const char *name;
    void (*handler)(int argc, char args[][MAX_ARG_LENGTH]);
    const char *description;
    const char *usage;
} shell_command_t;

typedef struct {
    char command_buffer[MAX_INPUT_LENGTH];
    char input_buffer[MAX_INPUT_LENGTH];
    char history[MAX_HISTORY_ENTRIES][MAX_INPUT_LENGTH];
    int history_index;
    int history_current;
    int input_index;
    int cursor_pos;
    int cursor_blink_counter;
    int cursor_visible;
} shell_state_t;

extern volatile uint8_t received_key;
extern shell_state_t shell_state;

void shell_init(void);
void shell_print_prompt(void);
void shell_redraw_input(void);
void shell_update_cursor(void);
void shell_newline(void);
void shell_scroll_up(void);

void shell_print(const char *text);
void shell_print_color(const char *text, uint32_t color);
void shell_printf(const char *format, ...);
void shell_error(const char *text);
void shell_success(const char *text);

char *input(char received);
void shell_backspace(void);
void shell_delete(void);
void shell_move_cursor_left(void);
void shell_move_cursor_right(void);
void shell_move_cursor_home(void);
void shell_move_cursor_end(void);
void shell_clear_input(void);
void shell_cancel_input(void);
char wait_for_input(void);

void shell_history_up(void);
void shell_history_down(void);

void parse_args(const char *command, char args[][MAX_ARG_LENGTH], int *argc);
void parse_command(void);

void cmd_help(int argc, char args[][MAX_ARG_LENGTH]);
void cmd_hello(int argc, char args[][MAX_ARG_LENGTH]);
void cmd_clear(int argc, char args[][MAX_ARG_LENGTH]);
void cmd_echo(int argc, char args[][MAX_ARG_LENGTH]);
void cmd_history(int argc, char args[][MAX_ARG_LENGTH]);
void cmd_uptime(int argc, char args[][MAX_ARG_LENGTH]);
void cmd_format(int argc, char args[][MAX_ARG_LENGTH]);
void cmd_lsdri(int argc, char args[][MAX_ARG_LENGTH]);
void cmd_mkfile(int argc, char args[][MAX_ARG_LENGTH]);
void cmd_lsf(int argc, char args[][MAX_ARG_LENGTH]);
void cmd_pause(int argc, char args[][MAX_ARG_LENGTH]);
void cmd_cat(int argc, char args[][MAX_ARG_LENGTH]);
void cmd_exit(int argc, char args[][MAX_ARG_LENGTH]);

extern const shell_command_t commands[];

#endif