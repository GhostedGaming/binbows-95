#include <serial.h>
#include <mem.h>
#include <framebuffer.h>
#include <shell.h>

void parse_args(const char *command, char args[][MAX_ARG_LENGTH], int *argc) {
    *argc = 0;
    const char *p = command;
    
    while (*p && *argc < MAX_ARGS) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;
        
        int j = 0;
        while (*p && *p != ' ' && *p != '\t' && j < MAX_ARG_LENGTH - 1) {
            args[*argc][j++] = *p++;
        }
        args[*argc][j] = '\0';
        (*argc)++;
    }
}

void parse_command(void) {
    if (!shell_state.command_buffer[0]) {
        shell_print_prompt();
        shell_redraw_input();
        return;
    }
    
    char args[MAX_ARGS][MAX_ARG_LENGTH];
    int argc;
    parse_args(shell_state.command_buffer, args, &argc);
    
    if (argc == 0) {
        shell_print_prompt();
        shell_redraw_input();
        return;
    }
    
    const shell_command_t *cmd = commands;
    while (cmd->name && strcmp(cmd->name, args[0]) != 0) {
        cmd++;
    }
    
    if (cmd->name) {
        cmd->handler(argc, args);
        if (strcmp(args[0], "exit") == 0) {
            return;
        }
    } else {
        shell_error("Unknown command");
        shell_print("Type 'help' for available commands.\n");
    }
    
    shell_state.command_buffer[0] = '\0';
    shell_print_prompt();
    shell_redraw_input();
}