#include <shell.h>
#include <stddef.h>
#include <util.h>
#include <framebuffer.h>
#include <scheduler.h>
#include <mem.h>
#include <limine.h>
#include <bexe.h>
#include <fat12.h>

extern volatile struct limine_module_request module_request;

void cmd_hello(int argc, char args[][MAX_ARG_LENGTH]);
void cmd_help(int argc, char args[][MAX_ARG_LENGTH]);
void cmd_clear(int argc, char args[][MAX_ARG_LENGTH]);
void cmd_echo(int argc, char args[][MAX_ARG_LENGTH]);
void cmd_history(int argc, char args[][MAX_ARG_LENGTH]);
void cmd_uptime(int argc, char args[][MAX_ARG_LENGTH]);
void cmd_exit(int argc, char args[][MAX_ARG_LENGTH]);
void cmd_pause(int argc, char args[][MAX_ARG_LENGTH]);
void cmd_ps(int argc, char args[][MAX_ARG_LENGTH]);
void cmd_kill(int argc, char args[][MAX_ARG_LENGTH]);
void cmd_nice(int argc, char args[][MAX_ARG_LENGTH]);
void cmd_ls(int argc, char args[][MAX_ARG_LENGTH]);
void cmd_cat(int argc, char args[][MAX_ARG_LENGTH]);
void cmd_exec(int argc, char args[][MAX_ARG_LENGTH]);
void cmd_format(int argc, char args[][MAX_ARG_LENGTH]);

const shell_command_t commands[] = {
    {"hello", cmd_hello, "Display greeting", "hello [name]"},
    {"help", cmd_help, "Show help message", "help"},
    {"clear", cmd_clear, "Clear screen", "clear"},
    {"echo", cmd_echo, "Echo text", "echo <text>"},
    {"history", cmd_history, "Show command history", "history"},
    {"uptime", cmd_uptime, "Show system uptime", "uptime"},
    {"exit", cmd_exit, "Exit shell", "exit"},
    {"pause", cmd_pause, "Pause until input", "pause"},
    {"ps", cmd_ps, "List processes", "ps"},
    {"kill", cmd_kill, "Terminate process by PID", "kill <pid>"},
    {"nice", cmd_nice, "Change process priority", "nice <pid> <priority>"},
    {"format", cmd_format, "Format a drive with FAT12", "format <drive>"},
    {"ls", cmd_ls, "List files on FAT12 drive", "ls [drive]"},
    {"cat", cmd_cat, "Display file contents from FAT12 drive", "cat <drive> <filename>"},
    {"exec", cmd_exec, "Execute a BEXE program from FAT12", "exec <filename>"},
    {NULL, NULL, NULL, NULL}
};

void cmd_ls(int argc, char args[][MAX_ARG_LENGTH]) {
    uint8_t drive = 0;
    if (argc > 1) {
        drive = args[1][0] - '0';
    }
    
    shell_printf("Listing files on drive %u:\n", drive);
    char* files = fat12_read_files(drive);
    if (files) {
        shell_print(files);
        kfree(files);
    } else {
        shell_error("Failed to read directory\n");
    }
}

void cmd_cat(int argc, char args[][MAX_ARG_LENGTH]) {
    if (argc < 3) {
        shell_error("cat: missing arguments\n");
        shell_print("Usage: cat <drive> <filename>\n");
        return;
    }

    uint8_t drive = args[1][0] - '0';
    const char* filename = args[2];
    
    uint32_t file_size = 0;
    uint8_t* file_content = fat12_read_file(drive, filename, &file_size);

    if (!file_content) {
        shell_printf("File not found: %s\n", filename);
    } else {
        char* buffer = (char*)kmalloc(file_size + 1);
        if (buffer) {
            memcpy(buffer, file_content, file_size);
            buffer[file_size] = '\0';
            shell_print(buffer);
            if (file_size > 0 && buffer[file_size - 1] != '\n') {
                shell_print("\n");
            }
            kfree(buffer);
        }
        kfree(file_content);
    }
}

void cmd_exec(int argc, char args[][MAX_ARG_LENGTH]) {
    if (argc < 2) {
        shell_error("exec: missing filename\n");
        shell_print("Usage: exec <filename>\n");
        return;
    }

    const char* filename = args[1];
    
    shell_printf("Loading BEXE file: %s from drive 0\n", filename);
    
    void* vm = bexe_load(0, filename);
    
    if (!vm) {
        shell_error("exec: failed to load BEXE file\n");
        return;
    }
    
    shell_print("Executing...\n");
    
    bexe_execute(vm);
    
    shell_print("Execution completed.\n");
    
    bexe_free(vm);
}

void cmd_ps(int argc, char args[][MAX_ARG_LENGTH]) {
    (void)argc; (void)args;
    shell_print("PID\tState\tPriority\tEntry\n");
    uint32_t count = get_process_count();
    for (uint32_t i = 0; i < count; i++) {
        process_t *p = get_process_at(i);
        if (!p) continue;
        const char *state = "?";
        switch (p->state) {
            case PROCESS_READY: state = "READY"; break;
            case PROCESS_RUNNING: state = "RUN"; break;
            case PROCESS_BLOCKED: state = "BLKD"; break;
            case PROCESS_TERMINATED: state = "TERM"; break;
        }
        shell_printf("%u\t%s\t%u\t%p\n", p->pid, state, p->priority, p->entry_point);
    }
}

void cmd_kill(int argc, char args[][MAX_ARG_LENGTH]) {
    if (argc < 2) {
        shell_error("kill: missing pid\n");
        return;
    }
    uint32_t pid = 0;
    for (char *p = args[1]; *p; p++) {
        if (*p < '0' || *p > '9') { shell_error("kill: invalid pid\n"); return; }
        pid = pid * 10 + (*p - '0');
    }
    if (pid == 0) {
        shell_error("kill: cannot kill kernel (pid 0)\n");
        return;
    }
    terminate_process(pid);
}

void cmd_help(int argc, char args[][MAX_ARG_LENGTH]) {
    (void)argc; (void)args;
    shell_printf("Available commands:\n");
    for (const shell_command_t *cmd = commands; cmd->name; cmd++) {
        shell_print("  ");
        shell_print(cmd->usage);
        shell_print(" - ");
        shell_print(cmd->description);
        shell_print("\n");
    }
}

void cmd_hello(int argc, char args[][MAX_ARG_LENGTH]) {
    shell_print("Hello, ");
    shell_print((argc > 1) ? args[1] : "World");
    shell_print("!\n");
}

void cmd_clear(int argc, char args[][MAX_ARG_LENGTH]) {
    (void)argc; (void)args;
    clear_screen();
    move_cursor_to(0, 0);
}

void cmd_echo(int argc, char args[][MAX_ARG_LENGTH]) {
    if (argc < 2) {
        shell_error("echo: missing argument\n");
        return;
    }
    for (int i = 1; i < argc; i++) {
        shell_print(args[i]);
        if (i < argc - 1) shell_print(" ");
    }
    shell_print("\n");
}

void cmd_history(int argc, char args[][MAX_ARG_LENGTH]) {
    (void)argc; (void)args;
    shell_print("Command history:\n");
    uint32_t num_entries = (shell_state.history_index > MAX_HISTORY_ENTRIES) ? MAX_HISTORY_ENTRIES : shell_state.history_index;
    uint32_t start_idx = (shell_state.history_index > MAX_HISTORY_ENTRIES) ? shell_state.history_index % MAX_HISTORY_ENTRIES : 0;
    if (num_entries == 0) {
        shell_print("  (no commands in history)\n");
        return;
    }
    for (uint32_t i = 0; i < num_entries; i++) {
        int idx = (start_idx + i) % MAX_HISTORY_ENTRIES;
        if (strlen(shell_state.history[idx]) > 0) {
            shell_printf("  %u: %s\n", shell_state.history_index - num_entries + i + 1, shell_state.history[idx]);
        }
    }
}

void cmd_uptime(int argc, char args[][MAX_ARG_LENGTH]) {
    (void)argc; (void)args;
    uint64_t ticks = get_timer_ticks();
    uint64_t seconds = ticks / 100;  // Assuming 100Hz timer
    uint64_t minutes = seconds / 60;
    uint64_t hours = minutes / 60;
    
    shell_printf("System uptime: %llu:%02llu:%02llu (%llu ticks)\n", 
                 hours, minutes % 60, seconds % 60, ticks);
}

void cmd_pause(int argc, char args[][MAX_ARG_LENGTH]) {
    (void)argc; (void)args;
    shell_print("Press any key to continue...");
    wait_for_input();
    shell_print("\n");
}

void cmd_exit(int argc, char args[][MAX_ARG_LENGTH]) {
    (void)argc; (void)args;
    shell_success("Goodbye!\n");
}

void cmd_nice(int argc, char args[][MAX_ARG_LENGTH]) {
    if (argc < 3) { shell_error("nice: missing arguments\n"); return; }
    uint32_t pid = 0;
    for (char *p = args[1]; *p; p++) { 
        if (*p < '0' || *p > '9') { shell_error("nice: invalid pid\n"); return; } 
        pid = pid * 10 + (*p - '0'); 
    }
    int newp = 0;
    int sign = 1;
    char *s = args[2];
    if (*s == '-') { sign = -1; s++; }
    for (; *s; s++) { 
        if (*s < '0' || *s > '9') { shell_error("nice: invalid priority\n"); return; } 
        newp = newp * 10 + (*s - '0'); 
    }
    newp *= sign;
    process_t *proc = get_process_by_pid(pid);
    if (!proc) { shell_error("nice: pid not found\n"); return; }
    if (proc->pid == 0) { shell_error("nice: cannot change kernel priority\n"); return; }
    if (newp < 0) proc->priority = 0; else proc->priority = (uint32_t)newp;
    shell_printf("Set PID %u priority to %u\n", proc->pid, proc->priority);
}

void cmd_format(int argc, char args[][MAX_ARG_LENGTH]) {
    if (argc < 2) {
        shell_error("format: missing drive number\n");
        shell_print("Usage: format <drive>\n");
        return;
    }

    uint8_t drive = args[1][0] - '0';
    
    shell_printf("Formatting drive %u with FAT12...\n", drive);
    format_fat12(drive, "BINBOWS ");
    shell_success("Format complete!\n");
}
