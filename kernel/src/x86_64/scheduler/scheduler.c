#include <scheduler.h>
#include <serial.h>
#include <mem.h>
#include <assert.h>
#include <util.h>

#define STACK_SIZE (16 * 4096)
#define DEFAULT_TIME_SLICE 2

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

    *(--stack) = (uint64_t)process->entry_point;
    *(--stack) = 0x202; // RFLAGS (popfq)
    *(--stack) = 0;     // rax
    *(--stack) = 0;     // rbx
    *(--stack) = 0;     // rcx
    *(--stack) = 0;     // rdx
    *(--stack) = 0;     // rbp
    *(--stack) = 0;     // r8
    *(--stack) = 0;     // r9
    *(--stack) = 0;     // r10
    *(--stack) = 0;     // r11
    *(--stack) = 0;     // r12
    *(--stack) = 0;     // r13
    *(--stack) = 0;     // r14
    *(--stack) = 0;     // r15

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
    scheduler.processes[0].stack_ptr = (uint64_t*)((uint8_t*)stack_base + STACK_SIZE / 2);

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
    
    scheduler.processes[idx].pid = scheduler.next_pid++;
    scheduler.processes[idx].state = PROCESS_READY;
    scheduler.processes[idx].priority = priority;
    scheduler.processes[idx].time_slice = DEFAULT_TIME_SLICE;
    scheduler.processes[idx].cpu_time_used = 0;
    scheduler.processes[idx].entry_point = entry_point;
    scheduler.processes[idx].stack_base = stack_base;

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

    for (int i = 0; i < scheduler.process_count; i++) {
        if (scheduler.processes[i].pid == pid) {

            scheduler.processes[i].state = PROCESS_TERMINATED;

            free_process_stack(scheduler.processes[i].stack_base);

            for (int j = i; j < scheduler.process_count - 1; j++) {
                scheduler.processes[j] = scheduler.processes[j + 1];
            }

            scheduler.process_count--;

            if (scheduler.current_process >= scheduler.process_count) {
                scheduler.current_process = 0;
            } else if (scheduler.current_process > i) {
                scheduler.current_process--;
            }

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

    for (int i = 0; i < scheduler.process_count; i++) {
        if (scheduler.processes[i].pid == pid) {
            return &scheduler.processes[i];
        }
    }
    return NULL;
}

static int find_next_process(void) {
    if (scheduler.process_count == 0) return -1;
    if (scheduler.process_count == 1) return 0;

    int current = scheduler.current_process;
    int next = (current + 1) % scheduler.process_count;
    int start = next;

    do {
        if (scheduler.processes[next].state == PROCESS_READY) {
            return next;
        }
        next = (next + 1) % scheduler.process_count;
    } while (next != start);

    if (scheduler.processes[current].state == PROCESS_RUNNING) {
        return current;
    }

    return 0;
}

void change_process(void) {
    if (!initialized) return;
    if (scheduler.process_count <= 1) return;

    asm volatile("cli");

    process_t* current_proc = &scheduler.processes[scheduler.current_process];
    
    if (current_proc->time_slice > 0) {
        current_proc->time_slice--;
    }
    
    if (current_proc->time_slice > 0) {
        asm volatile("sti");
        return;
    }

    int next_idx = find_next_process();

    if (next_idx == -1 || next_idx == scheduler.current_process) {
        current_proc->time_slice = DEFAULT_TIME_SLICE;
        asm volatile("sti");
        return;
    }

    process_t* next_proc = &scheduler.processes[next_idx];

    serial_printf("[SCHEDULER] Context switch: PID %d -> PID %d\n",
                  current_proc->pid, next_proc->pid);

    if (current_proc->state == PROCESS_RUNNING) {
        current_proc->state = PROCESS_READY;
    }
    
    current_proc->time_slice = DEFAULT_TIME_SLICE;
    next_proc->time_slice = DEFAULT_TIME_SLICE;
    next_proc->state = PROCESS_RUNNING;
    
    scheduler.current_process = next_idx;

    context_switch(&current_proc->stack_ptr, &next_proc->stack_ptr);

    asm volatile("sti");
}

void yield(void) {
    process_t* current = get_current_process();
    if (current) {
        serial_printf("[SCHEDULER] Process PID=%d yielding\n", current->pid);
    }
    change_process();
}

void test_scheduler(void) {
    serial_printf("[TEST] Process started! Getting current process...\n");
    process_t* current = get_current_process();
    serial_printf("[TEST] Current process retrieved: PID=%d\n", current ? current->pid : -1);
    
    int counter = 0;
    while (1) {
        current = get_current_process();
        serial_printf("[TEST] PID=%d running (iteration %d)\n", 
                     current ? current->pid : -1, counter++);
        
        for (volatile int i = 0; i < 1000000; i++);
    }
}