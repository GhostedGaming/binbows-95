#include <scheduler.h>
#include <serial.h>
#include <mem.h>
#include <assert.h>
#include <util.h>
#include <stub.h>
#include <ps2_keyboard.h>

#define STACK_SIZE (16 * 4096)
#define DEFAULT_TIME_SLICE 3

volatile bool scheduler_tick = false;

static scheduler_t scheduler = {0};
static bool initialized = false;

uint64_t* allocate_process_stack(void) {
    uint64_t* stack = (uint64_t*)kmalloc(STACK_SIZE);
    if (!stack) {
        serial_printf("[SCHEDULER] ERROR: Failed to allocate process stack!\n");
        return NULL;
    }
    serial_printf("[SCHEDULER] Allocated stack at %p\n", stack);
    return stack;
}

void free_process_stack(uint64_t* stack_base) {
    if (stack_base) {
        kfree(stack_base);
        serial_printf("[SCHEDULER] Freed stack at %p\n", stack_base);
    }
}

void setup_initial_stack(process_t* process) {
    uint64_t* stack = (uint64_t*)((uint8_t*)process->stack_base + STACK_SIZE);
    if (process->is_user) {
        uint64_t user_vbase = 0x400000;
        size_t stack_size = 16 * 4096;
        /* Allocate and map the user stack pages at user_vbase..user_vbase+stack_size */
        uint64_t first_phys = allocate_user_stack(process->pml4_phys, user_vbase, stack_size);
        if (first_phys == 0) {
            process->is_user = false;
        } else {
            process->user_stack_vaddr = user_vbase;
                /* allocate_user_stack maps [user_vaddr - size, user_vaddr)
                    so the top-of-stack is user_vaddr - 1. Use user_vbase - 8
                    as an aligned initial RSP within the mapped region. */
                uint64_t user_rsp = user_vbase - 8;
            uint64_t user_rip = (uint64_t)process->entry_point;
            /* When iret to a lower privilege, the CPU expects the stack layout
               with RIP, CS, RFLAGS, RSP, SS (RIP is pushed last). We build
               the stack so iretq will pop them in the correct order. */
                /* Build stack so context_switch's pops will restore registers and
                    popfq will remove the saved flags, leaving the IRET frame
                    (RIP, CS, RFLAGS_for_iret, RSP, SS) for iretq. */
                *(--stack) = 0x23;       // SS (user data selector)
                *(--stack) = user_rsp;   // RSP
                *(--stack) = 0x202;      // RFLAGS for iret
                *(--stack) = 0x1B;       // CS
                *(--stack) = user_rip;   // RIP
                *(--stack) = 0x202;      // Saved flags word for popfq
                /* Push register values in order so that the final top-of-stack is r15
                    and the pop sequence pop r15..pop rax will restore them correctly. */
                *(--stack) = 0; // rax
                *(--stack) = 0; // rbx
                *(--stack) = 0; // rcx
                *(--stack) = 0; // rdx
                *(--stack) = 0; // rbp
                *(--stack) = 0; // r8
                *(--stack) = 0; // r9
                *(--stack) = 0; // r10
                *(--stack) = 0; // r11
                *(--stack) = 0; // r12
                *(--stack) = 0; // r13
                *(--stack) = 0; // r14
                *(--stack) = 0; // r15
            process->stack_ptr = stack;
            return;
        }
    }

    *(--stack) = (uint64_t)process->entry_point;
    *(--stack) = 0x202;
    *(--stack) = 0;
    *(--stack) = 0;
    *(--stack) = 0;
    *(--stack) = 0;
    *(--stack) = 0;
    *(--stack) = 0;
    *(--stack) = 0;
    *(--stack) = 0;
    *(--stack) = 0;
    *(--stack) = 0;
    *(--stack) = 0;
    *(--stack) = 0;

    process->stack_ptr = stack;
}

void scheduler_init(void) {
    if (initialized) {
        serial_printf("[SCHEDULER] WARNING: Already initialized!\n");
        return;
    }

    scheduler.next_pid = 1;
    scheduler.current_process = 0;
    scheduler.process_count = 0;

    uint64_t* stack_base = allocate_process_stack();
    if (!stack_base) {
        serial_printf("[SCHEDULER] FATAL: Could not allocate stack for kernel process!\n");
        return;
    }

    scheduler.processes[0].pid = 0;
    scheduler.processes[0].state = PROCESS_RUNNING;
    scheduler.processes[0].priority = 0;
    scheduler.processes[0].time_slice = DEFAULT_TIME_SLICE;
    scheduler.processes[0].cpu_time_used = 0;
    scheduler.processes[0].stack_base = stack_base;
    scheduler.processes[0].entry_point = NULL;
    scheduler.processes[0].stack_ptr = (uint64_t*)((uint8_t*)stack_base + STACK_SIZE);
    scheduler.processes[0].is_user = false;
    // Kernel process pml4 is the current CR3
    {
        uint64_t cr3;
        asm volatile ("mov %%cr3, %0" : "=r"(cr3));
        scheduler.processes[0].pml4_phys = cr3 & ~0xFFFULL;
    }

    scheduler.process_count = 1;
    initialized = true;

    serial_printf("[SCHEDULER] Initialized with kernel process (PID 0)\n");
}

int create_process(void (*entry_point)(void), uint32_t priority) {
    if (!initialized) {
        serial_printf("[SCHEDULER] ERROR: Not initialized!\n");
        return -1;
    }

    if (scheduler.process_count >= MAX_PROCESS_COUNT) {
        serial_printf("[SCHEDULER] ERROR: Maximum process count reached!\n");
        return -1;
    }

    if (!entry_point) {
        serial_printf("[SCHEDULER] ERROR: NULL entry point!\n");
        return -1;
    }

    uint64_t* stack_base = allocate_process_stack();
    if (!stack_base) {
        return -1;
    }

    int idx = scheduler.process_count;

    uint32_t pid = scheduler.next_pid;
    while (get_process_by_pid(pid) != NULL) {
        pid++;
        if (pid == 0) pid = 1;
    }
    scheduler.processes[idx].pid = pid;
    scheduler.next_pid = pid + 1;
    if (scheduler.next_pid == 0) scheduler.next_pid = 1;
    scheduler.processes[idx].state = PROCESS_READY;
    scheduler.processes[idx].priority = priority;
    scheduler.processes[idx].time_slice = DEFAULT_TIME_SLICE;
    scheduler.processes[idx].cpu_time_used = 0;
    scheduler.processes[idx].entry_point = entry_point;
    scheduler.processes[idx].stack_base = stack_base;

    // Decide whether this is a kernel or user process. Kernel code lives in
    // the higher-half canonical addresses (sign bit set). If entry_point has
    // the top bit set, treat it as a kernel process and reuse the current CR3.
    uint64_t entry_addr = (uint64_t)(uintptr_t)entry_point;
    if ((entry_addr & (1ULL << 63)) != 0) {
        // Kernel process: use current CR3 and mark as kernel
        uint64_t cr3;
        asm volatile ("mov %%cr3, %0" : "=r"(cr3));
        scheduler.processes[idx].pml4_phys = cr3 & ~0xFFFULL;
        scheduler.processes[idx].is_user = false;
    } else {
        // User process: create per-process PML4
        uint64_t pml4 = create_user_pml4();
        if (pml4 == 0) {
            serial_printf("[SCHEDULER] WARNING: Failed to create user PML4 for PID=%d\n", scheduler.processes[idx].pid);
            uint64_t cr3;
            asm volatile ("mov %%cr3, %0" : "=r"(cr3));
            pml4 = cr3 & ~0xFFFULL;
        }
        scheduler.processes[idx].pml4_phys = pml4;
        scheduler.processes[idx].is_user = true;
    }

    // Copy tiny userspace stub into the new process address space at 0x400000
    if (scheduler.processes[idx].is_user) {
        uint64_t user_vbase = 0x400000;
        // Ensure the stack and necessary pages are allocated
    uint64_t first_phys = allocate_user_stack(scheduler.processes[idx].pml4_phys, user_vbase + 0x1000, 4096);
        (void)first_phys;
        // Map and copy the stub into the user mapping by temporarily loading pml4
    uint64_t old_cr3;
    asm volatile ("mov %%cr3, %0" : "=r"(old_cr3));
    load_pml4(scheduler.processes[idx].pml4_phys);
        void *dst = (void *)(user_vbase);
        for (unsigned int i = 0; i < userspace_stub_len; i++) {
            ((unsigned char *)dst)[i] = userspace_stub[i];
        }
    load_pml4(old_cr3 & ~0xFFFULL);

        /* store entry_point as a void* compatible address */
        scheduler.processes[idx].entry_point = (void (*)(void))(uintptr_t)user_vbase;
    }

    setup_initial_stack(&scheduler.processes[idx]);

    scheduler.process_count++;

    serial_printf("[SCHEDULER] Created process PID=%d, priority=%d, entry=%p\n",
                  scheduler.processes[idx].pid, priority, entry_point);

    return scheduler.processes[idx].pid;
}

void terminate_process(uint32_t pid) {
    if (!initialized) return;

    if (pid == 0) {
        serial_printf("[SCHEDULER] ERROR: Cannot terminate kernel process!\n");
        return;
    }

    for (uint32_t i = 0; i < scheduler.process_count; i++) {
        if (scheduler.processes[i].pid == pid) {

            scheduler.processes[i].state = PROCESS_TERMINATED;

            if (scheduler.processes[i].is_user && scheduler.processes[i].pml4_phys) {
                destroy_user_mappings(scheduler.processes[i].pml4_phys);
            }

            free_process_stack(scheduler.processes[i].stack_base);

            for (uint32_t j = i; j < scheduler.process_count - 1; j++) {
                scheduler.processes[j] = scheduler.processes[j + 1];
            }

            scheduler.process_count--;

            if (scheduler.current_process >= scheduler.process_count) {
                scheduler.current_process = 0;
            } else if (scheduler.current_process > i) {
                scheduler.current_process--;
            }

            if (keyboard_process_pid == pid) {
                disable_keyboard_irq();
                keyboard_process_pid = 0;
                serial_printf("[SCHEDULER] Keyboard process terminated: IRQ disabled\n");
            }

            serial_printf("[SCHEDULER] Terminated process PID=%d\n", pid);
            return;
        }
    }

    serial_printf("[SCHEDULER] WARNING: Process PID=%d not found\n", pid);
}

void block_process(uint32_t pid) {
    process_t* proc = get_process_by_pid(pid);
    if (proc && proc->state == PROCESS_RUNNING) {
        proc->state = PROCESS_BLOCKED;
        serial_printf("[SCHEDULER] Blocked process PID=%d\n", pid);
    }
}

void unblock_process(uint32_t pid) {
    process_t* proc = get_process_by_pid(pid);
    if (proc && proc->state == PROCESS_BLOCKED) {
        proc->state = PROCESS_READY;
        serial_printf("[SCHEDULER] Unblocked process PID=%d\n", pid);
    }
}

process_t* get_current_process(void) {
    if (!initialized || scheduler.current_process >= scheduler.process_count) {
        return NULL;
    }
    return &scheduler.processes[scheduler.current_process];
}

process_t* get_process_by_pid(uint32_t pid) {
    if (!initialized) return NULL;

    for (uint32_t i = 0; i < scheduler.process_count; i++) {
        if (scheduler.processes[i].pid == pid) {
            return &scheduler.processes[i];
        }
    }
    return NULL;
}

uint32_t get_process_count(void) {
    return scheduler.process_count;
}

process_t* get_process_at(uint32_t index) {
    if (!initialized) return NULL;
    if (index >= scheduler.process_count) return NULL;
    return &scheduler.processes[index];
}

static int find_next_process(void) {
    if (scheduler.process_count == 0) return -1;
    if (scheduler.process_count == 1) return 0;

    int current = scheduler.current_process;

    int best_idx = -1;
    uint32_t best_priority = UINT32_MAX;

    for (uint32_t i = 0; i < scheduler.process_count; i++) {
        uint32_t idx = (current + 1 + i) % scheduler.process_count;
        process_t *p = &scheduler.processes[idx];
        if (p->state != PROCESS_READY) continue;
        if (best_idx == -1 || p->priority < best_priority) {
            best_idx = (int)idx;
            best_priority = p->priority;
        }
    }

    if (best_idx != -1) return best_idx;

    if (scheduler.processes[current].state == PROCESS_RUNNING) return current;

    return 0;
}

void change_process(void) {
    if (!initialized) return;
    if (scheduler.process_count <= 1) return;

    asm volatile("cli");

    process_t* current_proc = &scheduler.processes[scheduler.current_process];
    
    if (current_proc->time_slice > 0) {
        current_proc->time_slice--;
        current_proc->cpu_time_used++;
    }
    
    if (current_proc->time_slice > 0 && current_proc->state == PROCESS_RUNNING) {
        asm volatile("sti");
        return;
    }

    int next_idx = find_next_process();

    if (next_idx == -1) {
        current_proc->time_slice = DEFAULT_TIME_SLICE;
        asm volatile("sti");
        return;
    }

    if ((uint32_t)next_idx == scheduler.current_process) {
        current_proc->time_slice = DEFAULT_TIME_SLICE;
        asm volatile("sti");
        return;
    }

    process_t* next_proc = &scheduler.processes[next_idx];

    //serial_printf("[SCHEDULER] Context switch: PID %d -> PID %d\n",
    //              current_proc->pid, next_proc->pid);

    if (current_proc->state == PROCESS_RUNNING) {
        current_proc->state = PROCESS_READY;
    }
    
    current_proc->time_slice = DEFAULT_TIME_SLICE;
    next_proc->time_slice = DEFAULT_TIME_SLICE;
    next_proc->state = PROCESS_RUNNING;
    
    scheduler.current_process = next_idx;

    asm volatile("sti");

    // Switch to next process's page table so its user stack is accessible.
    if (next_proc->pml4_phys) {
        load_pml4(next_proc->pml4_phys);
    }

    uint64_t cur_cr3;
    asm volatile ("mov %%cr3, %0" : "=r"(cur_cr3));
    serial_printf("[SCHEDULER] Switch PID %d -> %d (is_user=%d) pml4=0x%lx entry=%p stackptr=%p CR3=0x%lx\n",
                  current_proc->pid, next_proc->pid, next_proc->is_user,
                  next_proc->pml4_phys, next_proc->entry_point, next_proc->stack_ptr, cur_cr3);

    /* Dump first few qwords at the new stack pointer for debugging */
    if (next_proc->stack_ptr) {
        serial_printf("[SCHEDULER] next stack contents: ");
        for (int i = 0; i < 20; i++) {
            serial_printf("%016lx ", (uint64_t)next_proc->stack_ptr[i]);
        }
        serial_printf("\n");
    }

    uint64_t cr3_arg = next_proc->pml4_phys;
    if (next_proc->is_user) cr3_arg |= 1ULL;
    context_switch(&current_proc->stack_ptr, &next_proc->stack_ptr, cr3_arg);
}

void yield(void) {
    process_t* current = get_current_process();
    if (current) {
        serial_printf("[SCHEDULER] Process PID=%d yielding\n", current->pid);
        if (current->state == PROCESS_RUNNING) current->state = PROCESS_READY;
        current->time_slice = 0;
    }
    change_process();
}

void test_scheduler(void) {
    while (1) {
        for (volatile int i = 0; i < 1000000; i++);
    }
}