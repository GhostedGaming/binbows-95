#include <shell.h>
#include <stddef.h>

const shell_command_t commands[] = {
    {"hello", cmd_hello, "Display greeting", "hello [name]"},
    {"help", cmd_help, "Show help message", "help"},
    {"clear", cmd_clear, "Clear screen", "clear"},
    {"echo", cmd_echo, "Echo text", "echo <text>"},
    {"history", cmd_history, "Show command history", "history"},
    {"uptime", cmd_uptime, "Show system uptime", "uptime"},
    {"exit", cmd_exit, "Exit shell", "exit"},
    {NULL, NULL, NULL, NULL}  // Sentinel
};

// Command implementations
void cmd_help(int argc, char args[][MAX_ARG_LENGTH]) {
    shell_print("Available commands:\n");
    for (const shell_command_t *cmd = commands; cmd->name; cmd++) {
        shell_print("  ");
        shell_print(cmd->usage);
        shell_print(" - ");
        shell_print(cmd->description);
        shell_print("\n");
    }
    shell_print("\nNavigation:\n"
               "  Ctrl+C        - Cancel current input\n"
               "  Ctrl+L        - Clear screen\n"
               "  Ctrl+A        - Move to beginning of line\n"
               "  Ctrl+E        - Move to end of line\n"
               "  Ctrl+K        - Kill to end of line\n"
               "  Ctrl+U        - Kill entire line\n"
               "  Ctrl+W        - Kill word backward\n"
               "  Left/Right    - Move cursor\n"
               "  Home/End      - Move to line start/end\n");
}

void cmd_hello(int argc, char args[][MAX_ARG_LENGTH]) {
    shell_print("Hello, ");
    shell_print((argc > 1) ? args[1] : "World");
    shell_print("!\n");
}

void cmd_clear(int argc, char args[][MAX_ARG_LENGTH]) {
    clear_screen();
    move_cursor_to(0, 0);
}

void cmd_echo(int argc, char args[][MAX_ARG_LENGTH]) {
    if (argc < 2) {
        shell_error("echo: missing argument");
        return;
    }
    
    for (int i = 1; i < argc; i++) {
        shell_print(args[i]);
        if (i < argc - 1) shell_print(" ");
    }
    shell_print("\n");
}

void cmd_history(int argc, char args[][MAX_ARG_LENGTH]) {
    shell_print("Command history:\n");
    int count = 0;
    for (int i = 0; i < MAX_HISTORY_ENTRIES; i++) {
        int idx = (shell_state.history_index + i) % MAX_HISTORY_ENTRIES;
        if (strlen(shell_state.history[idx]) > 0) {
            shell_print("  ");
            shell_print(shell_state.history[idx]);
            shell_print("\n");
            count++;
        }
    }
    if (count == 0) {
        shell_print("  (no commands in history)\n");
    }
}

void cmd_uptime(int argc, char args[][MAX_ARG_LENGTH]) {
    shell_print("System uptime: Unknown (uptime not implemented)\n");
}

void cmd_exit(int argc, char args[][MAX_ARG_LENGTH]) {
    shell_success("Goodbye!");
}