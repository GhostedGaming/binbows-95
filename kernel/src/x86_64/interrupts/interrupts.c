#include <interrupts.h>
#include <pic.h>
#include <serial.h>
#include <ps2_keyboard.h>
#include <timer.h>
#include <io.h>
#include <scheduler.h>

// ============================================================================
// MSR Definitions for SYSCALL/SYSRET (for future use)
// ============================================================================
#define IA32_STAR           0xC0000081
#define IA32_LSTAR          0xC0000082
#define IA32_FMASK          0xC0000084
#define IA32_KERNEL_GS_BASE 0xC0000102

// ============================================================================
// IDT Structure and Data
// ============================================================================
__attribute__((aligned(0x10))) static idt_entry_t idt[256];
static idtr_t idtr;

// ============================================================================
// External Assembly ISR/IRQ Handlers
// ============================================================================

// CPU Exception handlers (INT 0-31)
extern void isr0(void), isr1(void), isr2(void), isr3(void);
extern void isr4(void), isr5(void), isr6(void), isr7(void);
extern void isr8(void), isr9(void), isr10(void), isr11(void);
extern void isr12(void), isr13(void), isr14(void), isr15(void);
extern void isr16(void), isr17(void), isr18(void), isr19(void);
extern void isr20(void), isr21(void), isr22(void), isr23(void);
extern void isr24(void), isr25(void), isr26(void), isr27(void);
extern void isr28(void), isr29(void), isr30(void), isr31(void);

// Hardware IRQ handlers (INT 32+)
extern void irq0(void);  // Timer (IRQ0 -> INT 32)
extern void irq1(void);  // Keyboard (IRQ1 -> INT 33)

// ============================================================================
// MSR Read/Write Functions
// ============================================================================

static inline uint64_t read_msr(uint32_t msr) {
    uint32_t low, high;
    __asm__ volatile ("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return ((uint64_t)high << 32) | low;
}

static inline void write_msr(uint32_t msr, uint64_t value) {
    uint32_t low = (uint32_t)(value & 0xFFFFFFFF);
    uint32_t high = (uint32_t)(value >> 32);
    __asm__ volatile ("wrmsr" : : "c"(msr), "a"(low), "d"(high));
}

// ============================================================================
// IDT Entry Management
// ============================================================================

/**
 * Add an entry to the IDT
 * @param vector Interrupt vector number (0-255)
 * @param isr Pointer to the interrupt service routine
 * @param flags Interrupt gate flags (type and attributes)
 */
static void idt_add_entry(uint8_t vector, void (*isr)(void), uint8_t flags) {
    uint64_t addr = (uint64_t)(uintptr_t)isr;
    
    idt[vector].isr_low    = addr & 0xFFFF;
    idt[vector].kernel_cs  = 0x08;  // Kernel code segment selector
    idt[vector].ist        = 0;     // No IST (Interrupt Stack Table)
    idt[vector].attributes = flags;
    idt[vector].isr_mid    = (addr >> 16) & 0xFFFF;
    idt[vector].isr_high   = (addr >> 32) & 0xFFFFFFFF;
    idt[vector].reserved   = 0;
}

// ============================================================================
// IDT Initialization Functions
// ============================================================================

/**
 * Initialize the IDT structure and clear all entries
 */
void idt_init(void) {
    // Clear all IDT entries
    for (int i = 0; i < 256; i++) {
        idt[i] = (idt_entry_t){0};
    }

    // Set up IDTR
    idtr.limit = sizeof(idt) - 1;
    idtr.base  = (uint64_t)&idt;

    write_serial("IDT: Structure initialized and cleared\n");
}

/**
 * Load the IDT into the CPU
 */
void idt_load(void) {
    __asm__ volatile ("lidt %0" : : "m"(idtr));
    write_serial("IDT: Loaded into CPU\n");
}

/**
 * Install all CPU exception handlers (INT 0-31)
 */
void install_exceptions(void) {
    void (*exceptions[])(void) = {
        isr0,  isr1,  isr2,  isr3,  isr4,  isr5,  isr6,  isr7,
        isr8,  isr9,  isr10, isr11, isr12, isr13, isr14, isr15,
        isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23,
        isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31
    };

    for (int i = 0; i < 32; i++) {
        // 0x8E = Present, DPL=0, 64-bit interrupt gate
        idt_add_entry(i, exceptions[i], 0x8E);
    }

    write_serial("IDT: CPU exception handlers (0-31) installed\n");
}

/**
 * Install a hardware IRQ handler
 * @param irq_vector Interrupt vector (32+ for hardware IRQs)
 * @param handler Pointer to the IRQ handler function
 */
void install_irq_common(uint8_t irq_vector, void (*handler)(void)) {
    // 0x8E = Present, DPL=0, 64-bit interrupt gate
    idt_add_entry(irq_vector, handler, 0x8E);
}

/**
 * Initialize timer interrupt (IRQ0 -> INT 32)
 */
void init_timer_irq(void) {
    install_irq_common(32, irq0);
    write_serial("IDT: Timer interrupt (IRQ0 -> INT 32) installed\n");
}

/**
 * Initialize keyboard interrupt (IRQ1 -> INT 33)
 */
void init_keyboard_irq(void) {
    install_irq_common(33, irq1);
    write_serial("IDT: Keyboard interrupt (IRQ1 -> INT 33) installed\n");
}

// ============================================================================
// Exception Handler
// ============================================================================

/**
 * C-level CPU exception handler
 * Called from assembly stub with interrupt number
 */
void isr_handler(uint64_t interrupt_number) {
    serial_printf("\n=== CPU EXCEPTION %lu ===\n", interrupt_number);

    switch(interrupt_number) {
        case 0:
            serial_printf("Exception: Division Error (#DE)\n");
            serial_printf("Cause: Division by zero or result too large\n");
            break;

        case 1:
            serial_printf("Exception: Debug (#DB)\n");
            serial_printf("Cause: Debug exception or single-step trap\n");
            break;

        case 3:
            serial_printf("Exception: Breakpoint (#BP)\n");
            serial_printf("Cause: INT3 instruction executed\n");
            break;

        case 6:
            serial_printf("Exception: Invalid Opcode (#UD)\n");
            serial_printf("Cause: Processor encountered an invalid or reserved opcode\n");
            break;

        case 8:
            serial_printf("Exception: Double Fault (#DF)\n");
            serial_printf("Cause: Exception occurred while handling another exception\n");
            serial_printf("CRITICAL: System stability compromised!\n");
            break;

        case 11:
            serial_printf("Exception: Segment Not Present (#NP)\n");
            serial_printf("Cause: Segment descriptor marked not present\n");
            break;

        case 12:
            serial_printf("Exception: Stack-Segment Fault (#SS)\n");
            serial_printf("Cause: Stack operation exceeded segment limit\n");
            break;

        case 13: {
            serial_printf("Exception: General Protection Fault (#GP)\n");
            serial_printf("Cause: Protection violation (memory access, privilege level, etc.)\n");
            
            // Note: Error code is on the stack but we don't retrieve it here
            // Could be extended to read error code for more detailed diagnostics
            break;
        }

        case 14: {
            uint64_t fault_addr;
            __asm__ volatile ("mov %%cr2, %0" : "=r"(fault_addr));
            
            serial_printf("Exception: Page Fault (#PF)\n");
            serial_printf("Faulting address: 0x%016lx\n", fault_addr);
            
            // Check common problematic addresses
            if (fault_addr == 0x0) {
                serial_printf("*** NULL POINTER DEREFERENCE ***\n");
            } else if (fault_addr == 0x100000) {
                serial_printf("*** Page fault at binary execution address ***\n");
                serial_printf("The page is not mapped or lacks execute permissions!\n");
            } else if (fault_addr < 0x1000) {
                serial_printf("*** Low memory access (likely null pointer + offset) ***\n");
            }
            
            // Note: Error code on stack contains P, W/R, U/S, RSVD, I/D bits
            // Could be extended to decode error code for detailed info
            break;
        }

        case 16:
            serial_printf("Exception: x87 Floating-Point Exception (#MF)\n");
            serial_printf("Cause: x87 FPU floating-point error\n");
            break;

        case 17:
            serial_printf("Exception: Alignment Check (#AC)\n");
            serial_printf("Cause: Unaligned memory access with AC flag set\n");
            break;

        case 18:
            serial_printf("Exception: Machine Check (#MC)\n");
            serial_printf("Cause: Hardware error detected by CPU\n");
            serial_printf("CRITICAL: Hardware failure!\n");
            break;

        case 19:
            serial_printf("Exception: SIMD Floating-Point Exception (#XM)\n");
            serial_printf("Cause: SSE/AVX floating-point error\n");
            break;

        default:
            serial_printf("Exception: Unhandled exception #%lu\n", interrupt_number);
            serial_printf("No specific handler implemented\n");
            break;
    }

    serial_printf("=== SYSTEM HALTED ===\n");
    serial_printf("The system has encountered a fatal error and must stop.\n\n");
    
    // Halt the system
    asm volatile (
        "cli\n"  // Disable interrupts
        "1: hlt\n"  // Halt
        "jmp 1b\n"  // Loop in case of NMI or other wake-up
    );
    
    __builtin_unreachable();
}

// ============================================================================
// Hardware Interrupt Handler
// ============================================================================

/**
 * C-level hardware IRQ handler
 * Called from assembly stub with IRQ vector number
 */
void irq_handler(uint64_t irq_number) {
    switch(irq_number) {
        case 32: // IRQ0 - PIT Timer
            on_irq0();  // Call timer tick handler
            change_process();
            serial_printf(".");
            break;

        case 33: // IRQ1 - PS/2 Keyboard
            keyboard_handler(NULL);
            break;

        default:
            serial_printf("Unhandled IRQ: %lu\n", irq_number);
            break;
    }

    // Send End of Interrupt (EOI) to PIC(s)
    if (irq_number >= 40) {
        // IRQ 8-15: Send EOI to slave PIC first
        outb(0xA0, 0x20);
    }
    // Always send EOI to master PIC
    outb(0x20, 0x20);
}