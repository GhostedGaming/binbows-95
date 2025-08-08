#include <shell.h>
#include <fat12.h>
#include <stddef.h>
#include <util.h>

extern struct ide_device ide_devices[4]; // Change the number for more ide drives

// Simple template for people who want to make their own commands

/* 
void cmd_name(int argc, char args[][MAX_ARG_LENGTH]) {

}
*/

const shell_command_t commands[] = {
    {"hello", cmd_hello, "Display greeting", "hello [name]"},
    {"help", cmd_help, "Show help message", "help"},
    {"clear", cmd_clear, "Clear screen", "clear"},
    {"echo", cmd_echo, "Echo text", "echo <text>"},
    {"history", cmd_history, "Show command history", "history"},
    {"uptime", cmd_uptime, "Show system uptime", "uptime"},
    {"exit", cmd_exit, "Exit shell", "exit"},
    {"format", cmd_format, "Format a drive", "format"},
    {"lsdri", cmd_lsdri, "list drives", "lsdri"},
    {"lsf", cmd_lsf, "list files", "lsf"},
    {"mkfile", cmd_mkfile, "make a file", "mkfile"},
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

void cmd_format(int argc, char args[][MAX_ARG_LENGTH]) {
    shell_print("Formatting drive: ");
    
    int drive_num = NULL;
    
    if (argc > 1) {
        drive_num = char_to_int(args[1][0]);
        if (drive_num == -1) {
            shell_print("Invalid drive number. Use 0-9.\n");
            return;
        }
    }

    if (argc > 2) {
        format_fat12((uint8_t)drive_num, args[2]);
    } else {
        format_fat12((uint8_t)drive_num, "drive");
    }

    serial_printf("Boot_Check");

    char drive_name[11] = "";
    uint8_t boot_check[512];
    if (ide_read_sectors(0, 1, 0, boot_check) == 0) {
        serial_printf("Boot sector verification:\n");
        serial_printf("Jump instruction: 0x%02X 0x%02X 0x%02X\n", boot_check[0], boot_check[1], boot_check[2]);
        serial_printf("OEM name: ");

        int name_index = 0;
        for (int i = 3; i < 11; i++) {
            write_serial_char(boot_check[i]);
            drive_name[name_index] = boot_check[i];
            name_index++;
        }

        shell_print(drive_name);

        write_serial_char('\n');
        serial_printf("Boot signature: 0x%02X%02X\n", boot_check[511], boot_check[510]);

        bpb_t12 *test_bpb = (bpb_t12 *)(boot_check + 11);
        serial_printf("BPB verification:\n");
        serial_printf("  bytes_per_sector: %u\n", test_bpb->bytes_per_sector);
        serial_printf("  sectors_per_cluster: %u\n", test_bpb->sectors_per_cluster);
        serial_printf("  num_fats: %u\n", test_bpb->num_fats);
        serial_printf("  fat_size_16: %u\n", test_bpb->fat_size_16);
        serial_printf("  root_entry_count: %u\n", test_bpb->root_entry_count);
    }

    shell_print_prompt();
}

void cmd_lsdri(int argc, char args[][MAX_ARG_LENGTH]) {
    for (int i = 0; i < 4; i++) {
        ide_devices[i].Reserved = 0;
        ide_identify(i / 2, i % 2);
        if (ide_devices[i].Reserved) {
            shell_printf("Found IDE drive %d: %s, Size: %u sectors\n",
                        i, ide_devices[i].Model, ide_devices[i].Size);
        }
    }
}

void cmd_lsf(int argc, char args[][MAX_ARG_LENGTH]) {
    if (argc < 2) {
        shell_print("Please pass the drive number\n");
        return;
    }

    uint8_t drive = (uint8_t)(args[1][0] - '0');
    shell_printf(fat12_read_files(drive) + '\n');
}

void cmd_mkfile(int argc, char args[][MAX_ARG_LENGTH]) {
    if (argc < 3) {
        shell_print("Usage: mkfile <drive_number> <filename>\n");
        return;
    }

    uint8_t drive = (uint8_t)(args[1][0] - '0');
    fat12_write_file(drive, args[2], (const uint8_t*)"");
    shell_printf("File made %s\n", args[2]);
}

void cmd_exit(int argc, char args[][MAX_ARG_LENGTH]) {
    shell_success("Goodbye!");
}