#include <shell.h>
#include <fat12.h>
#include <stddef.h>
#include <util.h>
#include <framebuffer.h>
#include <elixir.h>

extern struct ide_device ide_devices[4];

const shell_command_t commands[] = {
    {"hello", cmd_hello, "Display greeting", "hello [name]"},
    {"help", cmd_help, "Show help message", "help"},
    {"clear", cmd_clear, "Clear screen", "clear"},
    {"echo", cmd_echo, "Echo text", "echo <text>"},
    {"history", cmd_history, "Show command history", "history"},
    {"uptime", cmd_uptime, "Show system uptime", "uptime"},
    {"exit", cmd_exit, "Exit shell", "exit"},
    {"format", cmd_format, "Format a drive", "format <drive_number> [label]"},
    {"lsdri", cmd_lsdri, "List drives", "lsdri"},
    {"lsf", cmd_lsf, "List files", "lsf <drive_number>"},
    {"mkfile", cmd_mkfile, "Create file", "mkfile <drive_number> <filename>"},
    {"cat", cmd_cat, "Display file contents", "cat <drive_number> <filename>"},
    {"pause", cmd_pause, "Pause until input", "pause"},
    {NULL, NULL, NULL, NULL}
};

void cmd_help(int argc, char args[][MAX_ARG_LENGTH]) {
    shell_print("Available commands:\n");
    for (const shell_command_t *cmd = commands; cmd->name; cmd++) {
        shell_print("  ");
        shell_print(cmd->usage);
        shell_print(" - ");
        shell_print(cmd->description);
        shell_print("\n");
    }
    shell_print("\nKeyboard shortcuts:\n"
               "  Ctrl+C - Cancel input\n"
               "  Ctrl+L - Clear screen\n"
               "  Ctrl+A - Home\n"
               "  Ctrl+E - End\n"
               "  Ctrl+U - Clear line\n");
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
    shell_print("System uptime: Not implemented\n");
}

void cmd_format(int argc, char args[][MAX_ARG_LENGTH]) {
    if (argc < 2) {
        shell_error("format: missing drive number");
        return;
    }
    
    uint8_t drive_num = args[1][0] - '0';
    if (drive_num < 0 || drive_num > 9) {
        shell_error("format: invalid drive number");
        return;
    }
    
    const char *label = (argc > 2) ? args[2] : "DRIVE";
    
    shell_printf("Formatting drive %d with label '%s'...\n", drive_num, label);
    format_fat12((uint8_t)drive_num, label);
    shell_success("Format complete");
}

void cmd_lsdri(int argc, char args[][MAX_ARG_LENGTH]) {
    for (int i = 0; i < 4; i++) {
        ide_devices[i].Reserved = 0;
        ide_identify(i / 2, i % 2);
        if (ide_devices[i].Reserved) {
            shell_printf("Drive %d: %s (%d sectors)\n",
                        i, ide_devices[i].Model, ide_devices[i].Size);
        }
    }
}

void cmd_lsf(int argc, char args[][MAX_ARG_LENGTH]) {
    if (argc < 2) {
        shell_error("lsf: missing drive number");
        return;
    }
    
    uint8_t drive = (uint8_t)(args[1][0] - '0');
    if (drive > 9) {
        shell_error("lsf: invalid drive number");
        return;
    }
    
    char *file_list = fat12_read_files(drive);
    shell_print(file_list);
    shell_print("\n");
}

void cmd_mkfile(int argc, char args[][MAX_ARG_LENGTH]) {
    if (argc < 3) {
        shell_error("mkfile: missing arguments");
        shell_print("Usage: mkfile <drive_number> <filename> Optional: <content>\n");
        return;
    }
    
    uint8_t drive = (uint8_t)(args[1][0] - '0');
    if (drive > 3) {
        shell_error("mkfile: invalid drive number");
        return;
    }
    
    static char content_buffer[512];
    content_buffer[0] = '\0';
    uint32_t content_length = 0;
    
    if (argc >= 4) {
        for (int i = 3; i < argc; i++) {
            uint32_t arg_len = strlen(args[i]);
            if (content_length + arg_len + 1 >= sizeof(content_buffer)) {
                shell_error("mkfile: content too long (max 511 characters)");
                return;
            }
            
            strcat(content_buffer, args[i]);
            content_length += arg_len;
            
            if (i < argc - 1) {
                strcat(content_buffer, " ");
                content_length += 1;
            }
        }
    }
    
    if (content_length == 0) {
        content_length = 1;
        content_buffer[0] = '\0';
    }
    
    int result = fat12_write_file(drive, args[2], (const uint8_t*)content_buffer, content_length);
    
    if (result == 0) {
        shell_printf("Created file: %s (%d bytes)\n", args[2], content_length);
    } else {
        shell_error("Failed to create file");
        switch (result) {
            case -1:
                shell_print("Error: Could not read/validate filesystem\n");
                break;
            case -2:
                shell_print("Error: No free clusters available\n");
                break;
            case -3:
                shell_print("Error: No free directory entries\n");
                break;
            default:
                shell_printf("Error: Unknown error code %d\n", result);
                break;
        }
    }
}

void cmd_cat(int argc, char args[][MAX_ARG_LENGTH]) {
    if (argc < 3) {
        shell_error("cat: missing arguments");
        shell_print("Usage: cat <drive_number> <filename>\n");
        return;
    }
    
    uint8_t drive = (uint8_t)(args[1][0] - '0');
    if (drive > 3) {
        shell_error("cat: invalid drive number");
        return;
    }
    
    const char* filename = args[2];
    uint32_t file_size;
    
    uint8_t* file_contents = fat12_read_file(drive, filename, &file_size);
    
    if (file_contents == NULL) {
        shell_error("cat: file not found or could not be read");
        return;
    }
    
    if (file_size == 0) {
        shell_print("(empty file)\n");
        kfree(file_contents);
        return;
    }
    
    char* display_buffer = (char*)kmalloc(file_size + 1);
    if (!display_buffer) {
        shell_error("cat: failed to allocate display buffer");
        kfree(file_contents);
        return;
    }
    
    memcpy(display_buffer, file_contents, file_size);
    display_buffer[file_size] = '\0';
    
    for (uint32_t i = 0; i < file_size; i++) {
        char c = display_buffer[i];
        
        if ((c >= 32 && c <= 126) || c == '\n' || c == '\r' || c == '\t') {
            char temp[2] = {c, '\0'};
            shell_print(temp);
        } else {
            shell_printf("\\x%02x", (unsigned char)c);
        }
    }
    
    if (file_size > 0 && display_buffer[file_size - 1] != '\n') {
        shell_print("\n");
    }
    
    shell_printf("\n[File: %s, Size: %d bytes]\n", filename, file_size);
    
    kfree(file_contents);
    kfree(display_buffer);
}

void cmd_pause(int argc, char args[][MAX_ARG_LENGTH]) {
    shell_print("Press any key to continue...");
    wait_for_input();
    shell_print("\n");
}

void cmd_exit(int argc, char args[][MAX_ARG_LENGTH]) {
    shell_success("Goodbye!");
}