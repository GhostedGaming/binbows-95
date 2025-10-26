#include <syscall.h>
#include <framebuffer.h>
#include <scheduler.h>
#include <serial.h>
#include <fat12.h>
#include <mem.h>

extern uint8_t *current_vm_memory;
extern uint32_t current_vm_memory_size;

/* ------------------------------
 * File Handle Structure
 * ------------------------------
 * Tracks open files in kernel mode.
 */
typedef struct {
    bool in_use;           // Is this handle currently in use?
    uint8_t drive;         // FAT12 drive number
    char filename[13];     // Filename (8.3 format plus null)
    uint8_t *buffer;       // File buffer in memory
    uint32_t size;         // Size of the file
    uint32_t position;     // Current read/write position
} file_handle_t;

#define MAX_OPEN_FILES 16
static file_handle_t open_files[MAX_OPEN_FILES] = {0};

/* ------------------------------
 * Debug Macro
 * ------------------------------
 */
#define SYSCALL_DEBUG 1

#if SYSCALL_DEBUG
#define DBG_PRINT(fmt, ...) serial_printf("[SYSCALL][DBG] " fmt "\n", ##__VA_ARGS__)
#else
#define DBG_PRINT(fmt, ...)
#endif

/* ------------------------------
 * Print String to Framebuffer
 * ------------------------------
 * Simple wrapper syscall to print strings.
 */
void sys_print(const char *string) {
    uint64_t vm_addr = (uint64_t)string;
    if (vm_addr < current_vm_memory_size) {
        const char *real_string = (const char*)(current_vm_memory + vm_addr);
        DBG_PRINT("Printing string at VM addr 0x%llx: %s", vm_addr, real_string);
        fb_print(real_string, 0xFFFFFFFF);
    } else {
        DBG_PRINT("Invalid string address: 0x%llx", vm_addr);
    }
}

/* ------------------------------
 * Read a File from FAT12
 * ------------------------------
 * Allocates a buffer, reads a file into memory.
 */
uint8_t* sys_read_file(uint8_t drive, const char *file_name, uint32_t *size_out) {
    DBG_PRINT("Reading file '%s' on drive %d", file_name, drive);

    uint32_t buffer_size = 64 * 1024; // Allocate up to 64 KB
    uint8_t *buffer = (uint8_t*)kmalloc(buffer_size);
    if (!buffer) {
        DBG_PRINT("Failed to allocate buffer for file '%s'", file_name);
        *size_out = 0;
        return NULL;
    }

    if (!fat12_read_file_to_buffer(drive, file_name, buffer, size_out)) {
        DBG_PRINT("Failed to read file '%s'", file_name);
        kfree(buffer);
        *size_out = 0;
        return NULL;
    }

    DBG_PRINT("Read file '%s' (%u bytes)", file_name, *size_out);
    return buffer;
}

/* ------------------------------
 * Write a File to FAT12
 * ------------------------------
 */
bool sys_write_file(uint8_t drive, const char *file_name, const uint8_t *buffer, uint32_t size) {
    DBG_PRINT("Writing file '%s' (%u bytes) to drive %d", file_name, size, drive);

    if (!buffer || size == 0) {
        DBG_PRINT("Invalid buffer or size for write");
        return false;
    }

    int result = fat12_write_file(drive, file_name, buffer, size);
    if (result == 0) {
        DBG_PRINT("Successfully wrote file '%s'", file_name);
        return true;
    }

    DBG_PRINT("Failed to write file '%s', error code: %d", file_name, result);
    return false;
}

/* ------------------------------
 * Open a File
 * ------------------------------
 */
uint64_t sys_open_file(uint8_t drive, const char *file_name) {
    DBG_PRINT("Opening file '%s' on drive %d", file_name, drive);

    int handle_idx = -1;
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (!open_files[i].in_use) {
            handle_idx = i;
            break;
        }
    }
    if (handle_idx == -1) {
        DBG_PRINT("No free file handles available");
        return (uint64_t)-1;
    }

    uint32_t file_size = 0;
    uint8_t *file_buffer = fat12_read_file(drive, file_name, &file_size);
    if (!file_buffer) {
        DBG_PRINT("Failed to open file '%s'", file_name);
        return (uint64_t)-1;
    }

    open_files[handle_idx].in_use = true;
    open_files[handle_idx].drive = drive;
    open_files[handle_idx].buffer = file_buffer;
    open_files[handle_idx].size = file_size;
    open_files[handle_idx].position = 0;

    int name_len = 0;
    while (file_name[name_len] && name_len < 12) {
        open_files[handle_idx].filename[name_len] = file_name[name_len];
        name_len++;
    }
    open_files[handle_idx].filename[name_len] = '\0';

    DBG_PRINT("File opened '%s' (handle: %d, size: %u)", file_name, handle_idx, file_size);
    return (uint64_t)handle_idx;
}

/* ------------------------------
 * Close a File
 * ------------------------------
 */
bool sys_close_file(uint64_t file_handle) {
    if (file_handle >= MAX_OPEN_FILES) {
        DBG_PRINT("Invalid file handle: %llu", file_handle);
        return false;
    }

    file_handle_t *fh = &open_files[file_handle];
    if (!fh->in_use) {
        DBG_PRINT("File handle %llu is not in use", file_handle);
        return false;
    }

    if (fh->buffer) kfree(fh->buffer);

    DBG_PRINT("Closed file '%s' (handle: %llu)", fh->filename, file_handle);
    memset(fh, 0, sizeof(file_handle_t));
    return true;
}

/* ------------------------------
 * Memory Syscalls
 * ------------------------------
 */
void* sys_kmalloc(uint32_t size) {
    DBG_PRINT("Allocating %u bytes", size);
    return kmalloc(size);
}

void sys_kfree(void *ptr) {
    DBG_PRINT("Freeing memory at %p", ptr);
    kfree(ptr);
}

void* sys_memcpy(void *dest, const void *src, uint32_t n) {
    DBG_PRINT("Memcpy %u bytes from %p to %p", n, src, dest);
    return memcpy(dest, src, n);
}

int sys_memcmp(const void *s1, const void *s2, uint32_t n) {
    DBG_PRINT("Memcmp %u bytes between %p and %p", n, s1, s2);
    return memcmp(s1, s2, n);
}

void* sys_memset(void *s, int c, uint32_t n) {
    DBG_PRINT("Memset %u bytes at %p with %02x", n, s, c);
    return memset(s, c, n);
}

/* ------------------------------
 * Main Syscall Handler
 * ------------------------------
 */
uint64_t syscall_handler_c(uint64_t syscall_number, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5) {
    DBG_PRINT("Syscall %llu invoked", syscall_number);

    switch (syscall_number) {
        case SYS_RESERVED:
            DBG_PRINT("Reserved syscall called");
            return (uint64_t)-1;

        case SYS_FB_PRINT:
            sys_print((char*)arg1);
            return 0;

        case SYS_READ_FILE: {
            uint32_t size = 0;
            uint8_t *buffer = sys_read_file((uint8_t)arg1, (const char*)arg2, &size);
            if (buffer) {
                if (arg3) *(uint8_t**)arg3 = buffer;
                if (arg4) *(uint32_t*)arg4 = size;
                DBG_PRINT("Read file syscall successful, size=%u", size);
                return 0;
            }
            return (uint64_t)-1;
        }

        case SYS_WRITE_FILE:
            return sys_write_file((uint8_t)arg1, (const char*)arg2, (const uint8_t*)arg3, (uint32_t)arg4) ? 0 : (uint64_t)-1;

        case SYS_OPEN_FILE:
            return sys_open_file((uint8_t)arg1, (const char*)arg2);

        case SYS_CLOSE_FILE:
            return sys_close_file(arg1) ? 0 : (uint64_t)-1;

        case SYS_KMALLOC:
            return (uint64_t)sys_kmalloc((uint32_t)arg1);

        case SYS_KFREE:
            sys_kfree((void*)arg1);
            return 0;

        case SYS_MEMCPY:
            return (uint64_t)sys_memcpy((void*)arg1, (const void*)arg2, (uint32_t)arg3);

        case SYS_MEMCMP:
            return (uint64_t)sys_memcmp((const void*)arg1, (const void*)arg2, (uint32_t)arg3);

        case SYS_MEMSET:
            return (uint64_t)sys_memset((void*)arg1, (int)arg2, (uint32_t)arg3);

        default:
            DBG_PRINT("Unknown syscall number: %llu", syscall_number);
            return (uint64_t)-1;
    }
}
