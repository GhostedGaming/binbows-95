#ifndef SYSCALL_H
#define SYSCALL_H

#define SYS_RESERVED 0
#define SYS_FB_PRINT 1
#define SYS_READ_FILE 2
#define SYS_WRITE_FILE 3
#define SYS_OPEN_FILE 4
#define SYS_CLOSE_FILE 5
#define SYS_KMALLOC 6
#define SYS_KFREE 7
#define SYS_MEMCPY 8
#define SYS_MEMCMP 9
#define SYS_MEMSET 10

#include <stdint.h>

uint64_t syscall_handler_c(uint64_t syscall_number, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5);

#endif // SYSCALL_H