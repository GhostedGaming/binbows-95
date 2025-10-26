#include <bexe.h>
#include <mem.h>
#include <serial.h>
#include <fat12.h>
#include <syscall.h>

uint8_t *current_vm_memory = NULL;
uint32_t current_vm_memory_size = 0;

/* ------------------------------
 * Debugging
 * ------------------------------
 */
#define BEXE_DEBUG 1

#if BEXE_DEBUG
#define DBG_PRINT(fmt, ...) serial_printf("[VM][DBG] " fmt "\n", ##__VA_ARGS__)
#else
#define DBG_PRINT(fmt, ...)
#endif

/* ------------------------------
 * Opcodes (matching assembler)
 * ------------------------------
 */
#define OP_MV       0x01
#define OP_ADD      0x02
#define OP_SUB      0x03
#define OP_MUL      0x04
#define OP_DIV      0x05
#define OP_JMP      0x06
#define OP_JE       0x07
#define OP_JNE      0x08
#define OP_JL       0x09
#define OP_JG       0x0A
#define OP_JLE      0x0B
#define OP_JGE      0x0C
#define OP_CMP      0x10
#define OP_PUSH     0x20
#define OP_POP      0x21
#define OP_CALL     0x30
#define OP_RET      0x31
#define OP_HLT      0x40
#define OP_INT      0x50
#define OP_NOP      0x90

/* ------------------------------
 * Instruction Modes
 * ------------------------------
 */
#define MODE_REG_REG 0x00
#define MODE_REG_IMM 0x01

/* ------------------------------
 * Register Access Macros
 * ------------------------------
 */
#define GET_REG(vm, idx) ( \
    (idx) == 0 ? (vm)->rax : \
    (idx) == 1 ? (vm)->rbx : \
    (idx) == 2 ? (vm)->rcx : \
    (idx) == 3 ? (vm)->rdx : \
    (idx) == 4 ? (vm)->rsp : \
    (idx) == 5 ? (vm)->rbp : \
    (idx) == 6 ? (vm)->rsi : \
    (idx) == 7 ? (vm)->rdi : \
    (idx) == 8 ? (vm)->r8  : \
    (idx) == 9 ? (vm)->r9  : \
    (idx) == 10 ? (vm)->r10 : \
    (idx) == 11 ? (vm)->r11 : \
    (idx) == 12 ? (vm)->r12 : \
    (idx) == 13 ? (vm)->r13 : \
    (idx) == 14 ? (vm)->r14 : \
    (vm)->r15 \
)

#define SET_REG(vm, idx, val) do { \
    if ((idx) == 0) (vm)->rax = (val); \
    else if ((idx) == 1) (vm)->rbx = (val); \
    else if ((idx) == 2) (vm)->rcx = (val); \
    else if ((idx) == 3) (vm)->rdx = (val); \
    else if ((idx) == 4) (vm)->rsp = (val); \
    else if ((idx) == 5) (vm)->rbp = (val); \
    else if ((idx) == 6) (vm)->rsi = (val); \
    else if ((idx) == 7) (vm)->rdi = (val); \
    else if ((idx) == 8) (vm)->r8  = (val); \
    else if ((idx) == 9) (vm)->r9  = (val); \
    else if ((idx) == 10) (vm)->r10 = (val); \
    else if ((idx) == 11) (vm)->r11 = (val); \
    else if ((idx) == 12) (vm)->r12 = (val); \
    else if ((idx) == 13) (vm)->r13 = (val); \
    else if ((idx) == 14) (vm)->r14 = (val); \
    else (vm)->r15 = (val); \
} while(0)

/* ------------------------------
 * Read Helpers
 * ------------------------------
 */
static inline uint8_t read_u8(vm_t *vm) {
    if (vm->ip >= vm->code_size) {
        vm->running = false;
        DBG_PRINT("IP out of bounds at 0x%x", vm->ip);
        return 0;
    }
    return vm->memory[vm->ip++];
}

static inline uint32_t read_u32(vm_t *vm) {
    if (vm->ip + 4 > vm->code_size) {
        vm->running = false;
        DBG_PRINT("IP out of bounds reading u32 at 0x%x", vm->ip);
        return 0;
    }
    uint32_t val = *(uint32_t*)(&vm->memory[vm->ip]);
    vm->ip += 4;
    return val;
}

/* ------------------------------
 * Stack Operations
 * ------------------------------
 */
static inline void push(vm_t *vm, uint64_t value) {
    vm->rsp -= 8;
    if (vm->rsp < vm->code_size + vm->data_size) {
        vm->running = false;
        DBG_PRINT("Stack overflow at RSP=0x%llx", vm->rsp);
        return;
    }
    *(uint64_t*)(vm->memory + vm->rsp) = value;
    DBG_PRINT("PUSH 0x%llx -> [RSP=0x%llx]", value, vm->rsp);
}

static inline uint64_t pop(vm_t *vm) {
    if (vm->rsp >= vm->memory_size - 8) {
        vm->running = false;
        DBG_PRINT("Stack underflow at RSP=0x%llx", vm->rsp);
        return 0;
    }
    uint64_t val = *(uint64_t*)(vm->memory + vm->rsp);
    vm->rsp += 8;
    DBG_PRINT("POP 0x%llx <- [RSP=0x%llx]", val, vm->rsp - 8);
    return val;
}

/* ------------------------------
 * Flags
 * ------------------------------
 */
static void update_flags(vm_t *vm, int64_t result) {
    vm->flags = 0;
    if (result == 0) vm->flags |= FLAG_ZERO;
    if (result < 0)  vm->flags |= FLAG_SIGN | FLAG_LESS;
    if (result > 0)  vm->flags |= FLAG_GREATER;
    DBG_PRINT("FLAGS updated: ZF=%d SF=%d LF=%d GF=%d",
              !!(vm->flags & FLAG_ZERO),
              !!(vm->flags & FLAG_SIGN),
              !!(vm->flags & FLAG_LESS),
              !!(vm->flags & FLAG_GREATER));
}

/* ------------------------------
 * Syscall
 * ------------------------------
 */
static void execute_syscall(vm_t *vm, uint8_t num) {
    DBG_PRINT("SYSCALL 0x%02x with RAX=0x%llx RBX=0x%llx RCX=0x%llx RDX=0x%llx RSI=0x%llx RDI=0x%llx",
              num, vm->rax, vm->rbx, vm->rcx, vm->rdx, vm->rsi, vm->rdi);

    // Set global VM memory pointer for syscalls to use
    current_vm_memory = vm->memory;
    current_vm_memory_size = vm->memory_size;

    uint64_t arg1 = vm->rbx;
    uint64_t arg2 = vm->rcx;
    uint64_t arg3 = vm->rdx;
    uint64_t arg4 = vm->rsi;
    uint64_t arg5 = vm->rdi;

    uint64_t result = syscall_handler_c(num, arg1, arg2, arg3, arg4, arg5);
    vm->rax = result;
    DBG_PRINT("SYSCALL returned RAX=0x%llx", vm->rax);
}

/* ------------------------------
 * Instruction Execution
 * ------------------------------
 */
static bool execute_instruction(vm_t *vm) {
    if (!vm->running) return false;
    
    uint32_t ip_start = vm->ip;
    uint8_t opcode = read_u8(vm);
    
    DBG_PRINT("IP=0x%04x: Opcode=0x%02x", ip_start, opcode);

    switch (opcode) {
        case OP_MV: {
            uint8_t dst = read_u8(vm);
            uint8_t mode = read_u8(vm);
            
            if (mode == MODE_REG_REG) {
                uint8_t src = read_u8(vm);
                uint64_t value = GET_REG(vm, src);
                SET_REG(vm, dst, value);
                DBG_PRINT("  MV REG[%d] <- REG[%d] (0x%llx)", dst, src, value);
            } else if (mode == MODE_REG_IMM) {
                uint32_t imm = read_u32(vm);
                SET_REG(vm, dst, imm);
                DBG_PRINT("  MV REG[%d] <- IMM 0x%x", dst, imm);
            } else {
                serial_printf("[VM] Invalid MV mode: 0x%x\n", mode);
                vm->running = false;
                return false;
            }
            break;
        }

        case OP_ADD: {
            uint8_t dst = read_u8(vm);
            uint8_t mode = read_u8(vm);
            
            if (mode == MODE_REG_REG) {
                uint8_t src = read_u8(vm);
                uint64_t result = GET_REG(vm, dst) + GET_REG(vm, src);
                SET_REG(vm, dst, result);
                DBG_PRINT("  ADD REG[%d] += REG[%d] = 0x%llx", dst, src, result);
            } else if (mode == MODE_REG_IMM) {
                uint32_t imm = read_u32(vm);
                uint64_t result = GET_REG(vm, dst) + imm;
                SET_REG(vm, dst, result);
                DBG_PRINT("  ADD REG[%d] += IMM 0x%x = 0x%llx", dst, imm, result);
            }
            break;
        }

        case OP_SUB: {
            uint8_t dst = read_u8(vm);
            uint8_t mode = read_u8(vm);
            
            if (mode == MODE_REG_REG) {
                uint8_t src = read_u8(vm);
                uint64_t result = GET_REG(vm, dst) - GET_REG(vm, src);
                SET_REG(vm, dst, result);
                DBG_PRINT("  SUB REG[%d] -= REG[%d] = 0x%llx", dst, src, result);
            } else if (mode == MODE_REG_IMM) {
                uint32_t imm = read_u32(vm);
                uint64_t result = GET_REG(vm, dst) - imm;
                SET_REG(vm, dst, result);
                DBG_PRINT("  SUB REG[%d] -= IMM 0x%x = 0x%llx", dst, imm, result);
            }
            break;
        }

        case OP_MUL: {
            uint8_t dst = read_u8(vm);
            uint8_t mode = read_u8(vm);
            
            if (mode == MODE_REG_REG) {
                uint8_t src = read_u8(vm);
                uint64_t result = GET_REG(vm, dst) * GET_REG(vm, src);
                SET_REG(vm, dst, result);
                DBG_PRINT("  MUL REG[%d] *= REG[%d] = 0x%llx", dst, src, result);
            } else if (mode == MODE_REG_IMM) {
                uint32_t imm = read_u32(vm);
                uint64_t result = GET_REG(vm, dst) * imm;
                SET_REG(vm, dst, result);
                DBG_PRINT("  MUL REG[%d] *= IMM 0x%x = 0x%llx", dst, imm, result);
            }
            break;
        }

        case OP_DIV: {
            uint8_t dst = read_u8(vm);
            uint8_t mode = read_u8(vm);
            
            if (mode == MODE_REG_REG) {
                uint8_t src = read_u8(vm);
                uint64_t divisor = GET_REG(vm, src);
                if (divisor == 0) {
                    serial_printf("[VM] Division by zero!\n");
                    vm->running = false;
                    return false;
                }
                uint64_t result = GET_REG(vm, dst) / divisor;
                SET_REG(vm, dst, result);
                DBG_PRINT("  DIV REG[%d] /= REG[%d] = 0x%llx", dst, src, result);
            } else if (mode == MODE_REG_IMM) {
                uint32_t imm = read_u32(vm);
                if (imm == 0) {
                    serial_printf("[VM] Division by zero!\n");
                    vm->running = false;
                    return false;
                }
                uint64_t result = GET_REG(vm, dst) / imm;
                SET_REG(vm, dst, result);
                DBG_PRINT("  DIV REG[%d] /= IMM 0x%x = 0x%llx", dst, imm, result);
            }
            break;
        }

        case OP_CMP: {
            uint8_t r1 = read_u8(vm);
            uint8_t mode = read_u8(vm);
            
            if (mode == MODE_REG_REG) {
                uint8_t r2 = read_u8(vm);
                int64_t diff = (int64_t)GET_REG(vm, r1) - (int64_t)GET_REG(vm, r2);
                update_flags(vm, diff);
                DBG_PRINT("  CMP REG[%d]=0x%llx vs REG[%d]=0x%llx", r1, GET_REG(vm, r1), r2, GET_REG(vm, r2));
            } else if (mode == MODE_REG_IMM) {
                uint32_t imm = read_u32(vm);
                int64_t diff = (int64_t)GET_REG(vm, r1) - (int64_t)imm;
                update_flags(vm, diff);
                DBG_PRINT("  CMP REG[%d]=0x%llx vs IMM 0x%x", r1, GET_REG(vm, r1), imm);
            }
            break;
        }

        case OP_JMP: {
            uint32_t addr = read_u32(vm);
            DBG_PRINT("  JMP -> 0x%x", addr);
            vm->ip = addr;
            break;
        }

        case OP_JE: {
            uint32_t addr = read_u32(vm);
            if (vm->flags & FLAG_ZERO) {
                DBG_PRINT("  JE taken -> 0x%x", addr);
                vm->ip = addr;
            } else {
                DBG_PRINT("  JE not taken (continue to 0x%x)", vm->ip);
            }
            break;
        }

        case OP_JNE: {
            uint32_t addr = read_u32(vm);
            if (!(vm->flags & FLAG_ZERO)) {
                DBG_PRINT("  JNE taken -> 0x%x", addr);
                vm->ip = addr;
            } else {
                DBG_PRINT("  JNE not taken (continue to 0x%x)", vm->ip);
            }
            break;
        }

        case OP_JL: {
            uint32_t addr = read_u32(vm);
            if (vm->flags & FLAG_LESS) {
                DBG_PRINT("  JL taken -> 0x%x", addr);
                vm->ip = addr;
            } else {
                DBG_PRINT("  JL not taken (continue to 0x%x)", vm->ip);
            }
            break;
        }

        case OP_JG: {
            uint32_t addr = read_u32(vm);
            if (vm->flags & FLAG_GREATER) {
                DBG_PRINT("  JG taken -> 0x%x", addr);
                vm->ip = addr;
            } else {
                DBG_PRINT("  JG not taken (continue to 0x%x)", vm->ip);
            }
            break;
        }

        case OP_JLE: {
            uint32_t addr = read_u32(vm);
            if ((vm->flags & FLAG_ZERO) || (vm->flags & FLAG_LESS)) {
                DBG_PRINT("  JLE taken -> 0x%x", addr);
                vm->ip = addr;
            } else {
                DBG_PRINT("  JLE not taken (continue to 0x%x)", vm->ip);
            }
            break;
        }

        case OP_JGE: {
            uint32_t addr = read_u32(vm);
            if ((vm->flags & FLAG_ZERO) || (vm->flags & FLAG_GREATER)) {
                DBG_PRINT("  JGE taken -> 0x%x", addr);
                vm->ip = addr;
            } else {
                DBG_PRINT("  JGE not taken (continue to 0x%x)", vm->ip);
            }
            break;
        }

        case OP_PUSH: {
            uint8_t reg = read_u8(vm);
            uint64_t value = GET_REG(vm, reg);
            push(vm, value);
            DBG_PRINT("  PUSH REG[%d]=0x%llx", reg, value);
            break;
        }

        case OP_POP: {
            uint8_t reg = read_u8(vm);
            uint64_t value = pop(vm);
            SET_REG(vm, reg, value);
            DBG_PRINT("  POP REG[%d]=0x%llx", reg, value);
            break;
        }

        case OP_CALL: {
            uint32_t addr = read_u32(vm);
            push(vm, vm->ip);
            vm->ip = addr;
            DBG_PRINT("  CALL 0x%x (return addr=0x%x)", addr, (uint32_t)(vm->ip - 5));
            break;
        }

        case OP_RET: {
            uint32_t return_addr = (uint32_t)pop(vm);
            vm->ip = return_addr;
            DBG_PRINT("  RET -> 0x%x", vm->ip);
            break;
        }

        case OP_INT: {
            uint8_t int_num = read_u8(vm);
            DBG_PRINT("  INT 0x%02x", int_num);
            
            // Linux-style syscall: int 0x80 (128)
            // Syscall number is in RAX, arguments in RBX, RCX, RDX, RSI, RDI
            if (int_num == 0x80) {
                // Call syscall handler with syscall number from RAX
                execute_syscall(vm, (uint8_t)vm->rax);
            } else {
                // Direct syscall with interrupt number
                // For legacy compatibility, pass the interrupt number as syscall number
                // and registers as-is (RAX becomes arg1, RBX becomes arg2, etc.)
                DBG_PRINT("LEGACY INT with RAX=0x%llx RBX=0x%llx RCX=0x%llx RDX=0x%llx RSI=0x%llx",
                          vm->rax, vm->rbx, vm->rcx, vm->rdx, vm->rsi);
                uint64_t result = syscall_handler_c(int_num, vm->rax, vm->rbx, vm->rcx, vm->rdx, vm->rsi);
                vm->rax = result;
            }
            break;
        }

        case OP_HLT: {
            DBG_PRINT("  HLT - VM halted");
            vm->running = false;
            serial_printf("[VM] Program halted normally\n");
            return false;
        }

        case OP_NOP: {
            DBG_PRINT("  NOP");
            break;
        }

        default:
            serial_printf("[VM] Unknown opcode 0x%02x at IP=0x%x\n", opcode, ip_start);
            vm->running = false;
            return false;
    }

    return vm->running;
}

/* ------------------------------
 * VM Load
 * ------------------------------
 */
void* bexe_load(uint8_t drive, const char* filename) {
    DBG_PRINT("Loading BEXE file '%s' from drive %d", filename, drive);
    
    uint32_t file_size = 0;
    uint8_t* file_buffer = fat12_read_file(drive, filename, &file_size);
    if (!file_buffer) {
        serial_printf("[VM] Failed to read file '%s'\n", filename);
        return NULL;
    }
    
    if (file_size < 10) {  // Minimum header size
        serial_printf("[VM] File too small to be valid BEXE (got %d bytes, need at least 10)\n", file_size);
        kfree(file_buffer);
        return NULL;
    }

    // Parse header manually to avoid padding issues
    uint32_t magic = *(uint32_t*)(file_buffer);
    uint16_t code_size = *(uint16_t*)(file_buffer + 4);
    uint16_t data_size = *(uint16_t*)(file_buffer + 6);
    uint16_t bss_size = *(uint16_t*)(file_buffer + 8);
    
    if (magic != 0x42455845) {
        serial_printf("[VM] Invalid BEXE magic: 0x%08x (expected 0x42455845)\n", magic);
        serial_printf("[VM] First bytes: %02x %02x %02x %02x\n", 
                     file_buffer[0], file_buffer[1], file_buffer[2], file_buffer[3]);
        kfree(file_buffer);
        return NULL;
    }

    DBG_PRINT("BEXE header: code=%d bytes, data=%d bytes, bss=%d bytes",
              code_size, data_size, bss_size);

    // Allocate VM structure
    vm_t* vm = (vm_t*)kmalloc(sizeof(vm_t));
    memset(vm, 0, sizeof(vm_t));

    // Allocate memory: code + data + bss + stack (64KB)
    uint32_t total_mem = code_size + data_size + bss_size + 65536;
    vm->memory = (uint8_t*)kmalloc(total_mem);
    memset(vm->memory, 0, total_mem);

    // Copy code section (starts at byte 10)
    memcpy(vm->memory, file_buffer + 10, code_size);
    
    // Copy data section
    memcpy(vm->memory + code_size, file_buffer + 10 + code_size, data_size);

    // BSS is already zeroed

    kfree(file_buffer);

    // Initialize VM state
    vm->code_size = code_size;
    vm->data_size = data_size;
    vm->bss_size = bss_size;
    vm->memory_size = total_mem;
    vm->ip = 0;
    vm->rsp = total_mem;
    vm->rbp = total_mem;
    vm->running = true;
    vm->flags = 0;
    vm->instruction_count = 0;

    DBG_PRINT("VM initialized: total_memory=%d, stack_top=0x%llx", total_mem, vm->rsp);
    DBG_PRINT("First code bytes: %02x %02x %02x %02x", 
             vm->memory[0], vm->memory[1], vm->memory[2], vm->memory[3]);
    return vm;
}

/* ------------------------------
 * VM Execute
 * ------------------------------
 */
void bexe_execute(void* vm_ptr) {
    vm_t* vm = (vm_t*)vm_ptr;
    if (!vm) {
        serial_printf("[VM] NULL VM pointer\n");
        return;
    }

    serial_printf("[VM] Starting execution...\n");
    const uint32_t max_instructions = 10000000;
    
    while (vm->running && vm->instruction_count < max_instructions) {
        if (!execute_instruction(vm)) {
            break;
        }
        vm->instruction_count++;
    }

    if (vm->instruction_count >= max_instructions) {
        serial_printf("[VM] Execution limit reached (%d instructions)\n", max_instructions);
    }

    DBG_PRINT("VM execution finished. Instructions executed: %d", vm->instruction_count);
    serial_printf("[VM] Execution complete: %d instructions\n", vm->instruction_count);
}

/* ------------------------------
 * VM Free
 * ------------------------------
 */
void bexe_free(void* vm_ptr) {
    vm_t* vm = (vm_t*)vm_ptr;
    if (vm) {
        if (vm->memory) {
            kfree(vm->memory);
        }
        kfree(vm);
        DBG_PRINT("VM freed");
    }
}

/* ------------------------------
 * Print VM State
 * ------------------------------
 */
void bexe_print_state(vm_t* vm) {
    if (!vm) return;
    
    serial_printf("\n=== VM State ===\n");
    serial_printf("IP    = 0x%08x\n", vm->ip);
    serial_printf("FLAGS = 0x%02x [Z=%d S=%d L=%d G=%d]\n", 
                  vm->flags,
                  !!(vm->flags & FLAG_ZERO),
                  !!(vm->flags & FLAG_SIGN),
                  !!(vm->flags & FLAG_LESS),
                  !!(vm->flags & FLAG_GREATER));
    serial_printf("\nRegisters:\n");
    serial_printf("RAX = 0x%016llx    RBX = 0x%016llx\n", vm->rax, vm->rbx);
    serial_printf("RCX = 0x%016llx    RDX = 0x%016llx\n", vm->rcx, vm->rdx);
    serial_printf("RSI = 0x%016llx    RDI = 0x%016llx\n", vm->rsi, vm->rdi);
    serial_printf("RSP = 0x%016llx    RBP = 0x%016llx\n", vm->rsp, vm->rbp);
    serial_printf("R8  = 0x%016llx    R9  = 0x%016llx\n", vm->r8, vm->r9);
    serial_printf("R10 = 0x%016llx    R11 = 0x%016llx\n", vm->r10, vm->r11);
    serial_printf("R12 = 0x%016llx    R13 = 0x%016llx\n", vm->r12, vm->r13);
    serial_printf("R14 = 0x%016llx    R15 = 0x%016llx\n", vm->r14, vm->r15);
    serial_printf("\nInstructions executed: %d\n", vm->instruction_count);
    serial_printf("================\n\n");
}