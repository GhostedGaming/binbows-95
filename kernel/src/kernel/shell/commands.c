#include <shell.h>
#include <stddef.h>
#include <util.h>
#include <framebuffer.h>
#include <elixir.h>
#include <scheduler.h>
#include <mem.h>
#include <limine.h>

extern volatile struct limine_module_request module_request;

extern HBA_MEM *abar;

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
void cmd_format_elixir(int argc, char args[][MAX_ARG_LENGTH]);
void cmd_ls_elixir(int argc, char args[][MAX_ARG_LENGTH]);
void cmd_touch_elixir(int argc, char args[][MAX_ARG_LENGTH]);
void cmd_cat_elixir(int argc, char args[][MAX_ARG_LENGTH]);
void cmd_lsdisk(int argc, char args[][MAX_ARG_LENGTH]);
void cmd_install(int argc, char args[][MAX_ARG_LENGTH]);
void cmd_format(int argc, char args[][MAX_ARG_LENGTH]);
void cmd_rm_elixir(int argc, char args[][MAX_ARG_LENGTH]);
void cmd_mv_elixir(int argc, char args[][MAX_ARG_LENGTH]);
void cmd_mkdir_elixir(int argc, char args[][MAX_ARG_LENGTH]);
void cmd_rmdir_elixir(int argc, char args[][MAX_ARG_LENGTH]);
void cmd_ls(int argc, char args[][MAX_ARG_LENGTH]);

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
    {"lsdisk", cmd_lsdisk, "List connected AHCI drives and their sizes", "lsdisk"},
    {"install", cmd_install, "Install the OS to an AHCI drive", "install <drive_num>"},
    {"ls", cmd_ls, "List files and directories on the ElixirFS drive", "ls [path]"},
    {"touch", cmd_touch_elixir, "Create a file on the ElixirFS drive", "touch <filename> [content]"},
    {"cat", cmd_cat_elixir, "Display file contents from the ElixirFS drive", "cat <filename>"},
    {"format", cmd_format, "Format an AHCI drive with ElixirFS", "format <drive_num>"},
    {"rm", cmd_rm_elixir, "Delete a file from the ElixirFS drive", "rm <filename>"},
    {"mv", cmd_mv_elixir, "Rename a file on the ElixirFS drive", "mv <old_filename> <new_filename>"},
    {"mkdir", cmd_mkdir_elixir, "Create a directory on the ElixirFS drive", "mkdir <path>"},
    {"rmdir", cmd_rmdir_elixir, "Remove an empty directory from the ElixirFS drive", "rmdir <path>"},
    {NULL, NULL, NULL, NULL}
};

void cmd_lsdisk(int argc, char args[][MAX_ARG_LENGTH]) {
    (void)argc; (void)args;
    shell_print("Scanning for AHCI drives...\n");
    uint16_t* identify_data = kmalloc(512);
    if (!identify_data) {
        shell_error("Failed to allocate memory for identify data.");
        return;
    }

    for (int i = 0; i < 32; i++) {
        if (abar->pi & (1 << i)) {
            HBA_PORT *port = &abar->ports[i];
            uint8_t det = port->ssts & 0x0F;
            uint8_t ipm = (port->ssts >> 8) & 0x0F;

            if (det == HBA_PORT_DET_PRESENT && ipm == HBA_PORT_IPM_ACTIVE) {
                if (ahci_identify(port, identify_data)) {
                    char model[41];
                    for(int k = 0; k < 40; k += 2) {
                        model[k] = ((char*)identify_data)[27 * 2 + k + 1];
                        model[k+1] = ((char*)identify_data)[27 * 2 + k];
                    }
                    model[40] = 0;
                    // Trim whitespace from model string
                    char* p = model + 39;
                    while (p > model && *p == ' ') *p-- = 0;

                    uint64_t sectors = *(uint64_t*)(identify_data + 100);
                    shell_printf("Port %d: %s (%llu sectors)\n", i, model, sectors);
                } else {
                    shell_printf("Port %d: Device present, but failed to identify.\n", i);
                }
            }
        }
    }
    kfree(identify_data);
}

void cmd_install(int argc, char args[][MAX_ARG_LENGTH]) {
    if (argc < 2) {
        shell_error("install: missing drive number. Use 'lsdisk' to find drives.");
        return;
    }

    int port_num = args[1][0] - '0';
    if (port_num < 0 || port_num > 31) {
        shell_error("install: invalid drive number.");
        return;
    }

    HBA_PORT* port = &abar->ports[port_num];
    uint8_t det = port->ssts & 0x0F;
    uint8_t ipm = (port->ssts >> 8) & 0x0F;
    if (!(det == HBA_PORT_DET_PRESENT && ipm == HBA_PORT_IPM_ACTIVE)) {
        shell_error("install: no active device at that port.");
        return;
    }

    uint16_t* identify_data = kmalloc(512);
    if (!identify_data || !ahci_identify(port, identify_data)) {
        shell_error("install: could not identify drive.");
        if(identify_data) kfree(identify_data);
        return;
    }

    uint64_t total_sectors = *(uint64_t*)(identify_data + 100);
    uint64_t drive_size = total_sectors * 512;
    kfree(identify_data);

    shell_printf("Installing Binbows 95 to port %d\n", port_num);
    shell_printf("Drive size: %llu sectors (%llu MiB)\n", total_sectors, drive_size / (1024*1024));

    // Step 1: Check for kernel module
    shell_print("[1/5] Checking for kernel module...\n");
    if (module_request.response == NULL) {
        shell_error("install: No Limine modules available.");
        shell_print("Make sure your limine.cfg contains:\n");
        shell_print("    module_path: boot():/boot/kernel\n");
        return;
    }

    if (module_request.response->module_count == 0) {
        shell_error("install: No modules loaded by Limine.");
        return;
    }

    // Find the kernel module
    struct limine_file* kernel_module = module_request.response->modules[0];
    shell_printf("      Found kernel: %llu bytes\n", kernel_module->size);

    // Step 2: Format the drive
    shell_print("[2/5] Formatting drive with ElixirFS...\n");
    if (elixir_format(port, drive_size) != 0) {
        shell_error("install: failed to format drive.");
        return;
    }
    shell_print("      Format complete.\n");

    // Step 3: Mount filesystem
    shell_print("[3/5] Mounting filesystem...\n");
    elixir_fs_t* fs = elixir_mount(port);
    if (!fs) {
        shell_error("install: failed to mount new filesystem.");
        return;
    }
    shell_print("      Mounted successfully.\n");

    // Step 4: Write kernel
        shell_print("[4/5] Writing kernel to /kernel...\n");
        if (elixir_create_file(fs, "/kernel", ELIXIR_FILE_TYPE_FILE, (const char*)kernel_module->address,
                              kernel_module->size) != 0) {
            shell_error("install: failed to write kernel to disk.");
            elixir_unmount(fs);
            return;
        }
        shell_printf("      Wrote %llu bytes.\n", kernel_module->size);
    
        // Step 5: Write bootloader config
        shell_print("[5/5] Writing bootloader configuration...\n");
        const char* limine_conf_content =
            "timeout: 3\n"
            "/Binbows 95\n"
            "    protocol: limine\n"
            "    kernel_path: boot():/kernel\n";
    
        if (elixir_create_file(fs, "/limine.conf", ELIXIR_FILE_TYPE_FILE, limine_conf_content,
                              strlen(limine_conf_content)) != 0) {        shell_error("install: failed to write limine.conf to disk.");
        elixir_unmount(fs);
        return;
    }
    shell_print("      Configuration written.\n");

    elixir_unmount(fs);
    
    shell_print("\n");
    shell_success("Installation complete!");
    shell_print("\n");
    shell_print("Next steps:\n");
    shell_print("  1. Shutdown the VM/system\n");
    shell_print("  2. Run: limine bios-install disk.img\n");
    shell_print("  3. Boot from the hard drive\n");
}

void cmd_format_elixir(int argc, char args[][MAX_ARG_LENGTH]) {
    (void)argc; (void)args;
    shell_print("Formatting AHCI drive 0 with Elixir filesystem...\n");
    int result = elixir_format(&abar->ports[0], 65536 * 512); // Assuming 32MB drive
    if (result == 0) {
        shell_success("Format complete.");
    } else {
        shell_error("Failed to format drive.");
    }
}

void cmd_ls(int argc, char args[][MAX_ARG_LENGTH]) {
    const char* path = (argc > 1) ? args[1] : "/";

    elixir_fs_t* fs = elixir_mount(&abar->ports[0]);
    if (!fs) {
        shell_error("ls: could not mount filesystem.");
        return;
    }

    uint32_t num_entries = 0;
    elixir_file_entry_t* entries = elixir_list_directory(fs, path, &num_entries);

    if (!entries) {
        shell_printf("ls: %s: No such directory or cannot list.\n", path);
        elixir_unmount(fs);
        return;
    }

    shell_printf("Listing contents of %s: %u entries.\n", path, num_entries);
    shell_print("Type | Name                         | Size (bytes) | Start Block\n");
    shell_print("-----|------------------------------|--------------|------------\n");
    for (uint32_t i = 0; i < num_entries; ++i) {
        elixir_file_entry_t* fe = &entries[i];
        const char* type_str = (fe->type == ELIXIR_FILE_TYPE_DIRECTORY) ? "DIR " : "FILE";
        shell_printf("% -4s | % -28s | % -12llu | % -10llu\n", type_str, fe->name, fe->size_bytes, fe->start_block);
    }

    kfree(entries);
    elixir_unmount(fs);
}

void cmd_touch_elixir(int argc, char args[][MAX_ARG_LENGTH]) {
    if (argc < 2) {
        shell_error("touch: missing filename");
        return;
    }

    const char* filename = args[1];
    char content_buffer[1024] = {0};
    size_t content_len = 0;

    if (argc > 2) {
        for (int i = 2; i < argc; i++) {
            strcat(content_buffer, args[i]);
            if (i < argc - 1) {
                strcat(content_buffer, " ");
            }
        }
        content_len = strlen(content_buffer);
    } else {
        // Create an empty file if no content is provided
        content_len = 0;
    }

    elixir_fs_t* fs = elixir_mount(&abar->ports[0]);
    if (!fs) {
        shell_error("touch: could not mount filesystem.");
        return;
    }

    int result = elixir_create_file(fs, filename, ELIXIR_FILE_TYPE_FILE, content_buffer, content_len);
    if (result == 0) {
        shell_printf("Created file: %s\n", filename);
    } else {
        shell_printf("Failed to create file. Error code: %d\n", result);
    }

    elixir_unmount(fs);
}

void cmd_cat_elixir(int argc, char args[][MAX_ARG_LENGTH]) {
    if (argc < 2) {
        shell_error("cat: missing filename");
        return;
    }

    const char* filename = args[1];
    elixir_fs_t* fs = elixir_mount(&abar->ports[0]);
    if (!fs) {
        shell_error("cat: could not mount filesystem.");
        return;
    }

    size_t file_size;
    void* file_content = elixir_read_file(fs, filename, &file_size);

    if (!file_content) {
        shell_printf("File not found: %s\n", filename);
    } else {
        // Ensure null termination for printing
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

    elixir_unmount(fs);
}

void cmd_format(int argc, char args[][MAX_ARG_LENGTH]) {
    if (argc < 2) {
        shell_error("format: missing drive number.");
        return;
    }

    int port_num = args[1][0] - '0';
    if (port_num < 0 || port_num > 31) {
        shell_error("format: invalid drive number.");
        return;
    }

    HBA_PORT* port = &abar->ports[port_num];
    uint8_t det = port->ssts & 0x0F;
    uint8_t ipm = (port->ssts >> 8) & 0x0F;
    if (!(det == HBA_PORT_DET_PRESENT && ipm == HBA_PORT_IPM_ACTIVE)) {
        shell_error("format: no active device at that port.");
        return;
    }

    uint16_t* identify_data = kmalloc(512);
    if (!identify_data || !ahci_identify(port, identify_data)) {
        shell_error("format: could not identify drive.");
        if(identify_data) kfree(identify_data);
        return;
    }

    uint64_t total_sectors = *(uint64_t*)(identify_data + 100);
    uint64_t drive_size = total_sectors * 512;
    kfree(identify_data);

    shell_printf("Formatting drive %d with ElixirFS...\n", port_num);
    int result = elixir_format(port, drive_size);
    if (result == 0) {
        shell_success("Format complete.");
    } else {
        shell_error("Failed to format drive.");
    }
}

void cmd_rm_elixir(int argc, char args[][MAX_ARG_LENGTH]) {
    if (argc < 2) {
        shell_error("rm: missing filename");
        return;
    }

    const char* filename = args[1];
    elixir_fs_t* fs = elixir_mount(&abar->ports[0]);
    if (!fs) {
        shell_error("rm: could not mount filesystem.");
        return;
    }

    int result = elixir_delete_file(fs, filename);
    if (result == 0) {
        shell_printf("Deleted file: %s\n", filename);
    } else {
        shell_printf("Failed to delete file. Error code: %d\n", result);
    }

    elixir_unmount(fs);
}

void cmd_mv_elixir(int argc, char args[][MAX_ARG_LENGTH]) {
    if (argc < 3) {
        shell_error("mv: missing operands\nUsage: mv <old_filename> <new_filename>");
        return;
    }

    const char* old_filename = args[1];
    const char* new_filename = args[2];

    elixir_fs_t* fs = elixir_mount(&abar->ports[0]);
    if (!fs) {
        shell_error("mv: could not mount filesystem.");
        return;
    }

    int result = elixir_rename_file(fs, old_filename, new_filename);
    if (result == 0) {
        shell_printf("Renamed %s to %s\n", old_filename, new_filename);
    } else {
        shell_printf("Failed to rename file. Error code: %d\n", result);
    }

    elixir_unmount(fs);
}

void cmd_mkdir_elixir(int argc, char args[][MAX_ARG_LENGTH]) {
    if (argc < 2) {
        shell_error("mkdir: missing operand\nUsage: mkdir <path>");
        return;
    }

    const char* path = args[1];
    elixir_fs_t* fs = elixir_mount(&abar->ports[0]);
    if (!fs) {
        shell_error("mkdir: could not mount filesystem.");
        return;
    }

    int result = elixir_mkdir(fs, path);
    if (result == 0) {
        shell_printf("Created directory: %s\n", path);
    } else {
        shell_printf("Failed to create directory. Error code: %d\n", result);
    }

    elixir_unmount(fs);
}

void cmd_rmdir_elixir(int argc, char args[][MAX_ARG_LENGTH]) {
    if (argc < 2) {
        shell_error("rmdir: missing operand\nUsage: rmdir <path>");
        return;
    }

    const char* path = args[1];
    elixir_fs_t* fs = elixir_mount(&abar->ports[0]);
    if (!fs) {
        shell_error("rmdir: could not mount filesystem.");
        return;
    }

    int result = elixir_rmdir(fs, path);
    if (result == 0) {
        shell_printf("Removed directory: %s\n", path);
    } else {
        shell_printf("Failed to remove directory. Error code: %d\n", result);
    }

    elixir_unmount(fs);
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
        shell_error("kill: missing pid");
        return;
    }
    uint32_t pid = 0;
    for (char *p = args[1]; *p; p++) {
        if (*p < '0' || *p > '9') { shell_error("kill: invalid pid"); return; }
        pid = pid * 10 + (*p - '0');
    }
    if (pid == 0) {
        shell_error("kill: cannot kill kernel (pid 0)");
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
    shell_print("System uptime: Not implemented\n");
}

void cmd_pause(int argc, char args[][MAX_ARG_LENGTH]) {
    (void)argc; (void)args;
    shell_print("Press any key to continue...");
    wait_for_input();
    shell_print("\n");
}

void cmd_exit(int argc, char args[][MAX_ARG_LENGTH]) {
    (void)argc; (void)args;
    shell_success("Goodbye!");
}

void cmd_nice(int argc, char args[][MAX_ARG_LENGTH]) {
    if (argc < 3) { shell_error("nice: missing arguments"); return; }
    uint32_t pid = 0;
    for (char *p = args[1]; *p; p++) { if (*p < '0' || *p > '9') { shell_error("nice: invalid pid"); return; } pid = pid * 10 + (*p - '0'); }
    int newp = 0;
    int sign = 1;
    char *s = args[2];
    if (*s == '-') { sign = -1; s++; }
    for (; *s; s++) { if (*s < '0' || *s > '9') { shell_error("nice: invalid priority"); return; } newp = newp * 10 + (*s - '0'); }
    newp *= sign;
    process_t *proc = get_process_by_pid(pid);
    if (!proc) { shell_error("nice: pid not found"); return; }
    if (proc->pid == 0) { shell_error("nice: cannot change kernel priority"); return; }
    if (newp < 0) proc->priority = 0; else proc->priority = (uint32_t)newp;
    shell_printf("Set PID %u priority to %u\n", proc->pid, proc->priority);
}
