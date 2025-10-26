#include <syscall.h>
#include <framebuffer.h>

uint64_t syscall_handler_c(uint64_t syscall_number, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5) {
    switch (syscall_number) {
        case SYS_FB_PRINT:
            fb_print((const char*)arg1, 0xFFFFFFFF);
            return 0;
        default:
            fb_printf("[SYSCALL] Unknown syscall number: %d\n", syscall_number);
            return (uint64_t)-1;
    }
}
