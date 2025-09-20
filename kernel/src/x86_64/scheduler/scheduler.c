#include <scheduler.h>
#include <util.h>
#include <serial.h>
#include <assert.h>
#include <mem.h>
#include <util.h>

// This file doesnt do anything yet eventually i will figure out how schedulers work

#define STACK_SIZE 4096

static scheduler_t scheduler = {0};
bool initialized = false;

extern void save_register_and_switch(uint64_t current_stack_ptr, uint64_t next_stack_ptr);

void scheduler_init() {
    scheduler.next_pid = 1;
    scheduler.current_process = 0;
    scheduler.processes[0].pid = 0;
    scheduler.processes[0].state = PROCESS_READY;
    scheduler.processes[0].priority = 0;
    scheduler.processes[0].stack_ptr = NULL;
    scheduler.processes[0].stack_base = NULL;
    scheduler.processes[0].entry_point = NULL;
    scheduler.processes[0].time_slice = 10;
    scheduler.processes[0].cpu_time_used = 0;
    scheduler.process_count = 1;
    initialized = true;
}

uint32_t* allocate_process_stack(void) {
    uint32_t* stack = (uint32_t*)kmalloc(STACK_SIZE);
    if (!stack) {
        serial_printf("Failed to allocate process stack!\n");
        return NULL;
    }
    return stack;
}

void free_process_stack(uint32_t* stack_base) {
    if (stack_base) {
        kfree(stack_base);
    }
}

void setup_initial_stack(process_t* process) {
    uint32_t* stack = (uint32_t*)process->stack_ptr;
    *(--stack) = (uint32_t)process->entry_point;
    process->stack_ptr = stack;
}

void context_switch(uint64_t one, uint64_t two) {
    if (initialized != true) {
        serial_printf("Scheduler not initialized!\n");
        ASSERT(initialized == true);
        return;
    }
    if (one == two) return;
    save_register_and_switch(one, two);
}

int load_process(void *binary_data, size_t size) {
    scheduler.processes->code = (uint32_t)kmalloc(size);
    if (!scheduler.processes->code) return -1;

}

int create_process(void (*entry_point)(void), uint32_t priority) {
    if (initialized != true) {
        serial_printf("Scheduler not initialized!\n");
        ASSERT(initialized == true);
        return -1;
    }
    if (scheduler.process_count >= MAX_PROCESS_COUNT) {
        serial_printf("Max process count reached!\n");
        return -1;
    }
    uint32_t* stack_base = allocate_process_stack();
    if (!stack_base) {
        return -1;
    }
    int new_index = scheduler.process_count;
    scheduler.processes[new_index].pid = scheduler.next_pid++;
    scheduler.processes[new_index].state = PROCESS_READY;
    scheduler.processes[new_index].priority = priority;
    scheduler.processes[new_index].entry_point = entry_point;
    scheduler.processes[new_index].stack_base = stack_base;
    scheduler.processes[new_index].stack_ptr = stack_base + (STACK_SIZE / sizeof(uint32_t)) - 1;
    scheduler.processes[new_index].time_slice = 10;
    scheduler.processes[new_index].cpu_time_used = 0;
    setup_initial_stack(&scheduler.processes[new_index]);
    scheduler.process_count++;

    return scheduler.processes[new_index].pid;
}

void terminate_process(uint32_t pid) {
    for (int i = 0; i < scheduler.process_count; i++) {
        if (scheduler.processes[i].pid == pid) {
            free_process_stack(scheduler.processes[i].stack_base);
            for (int j = i; j < scheduler.process_count - 1; j++) {
                scheduler.processes[j] = scheduler.processes[j + 1];
            }
            scheduler.process_count--;
            if (scheduler.current_process > i) {
                scheduler.current_process--;
            } else if (scheduler.current_process == i) {
                scheduler.current_process = 0;
                change_process();
            }
            break;
        }
    }
}

void block_process(uint32_t pid) {
    for (int i = 0; i < scheduler.process_count; i++) {
        if (scheduler.processes[i].pid == pid) {
            scheduler.processes[i].state = PROCESS_BLOCKED;
            break;
        }
    }
}

void unblock_process(uint32_t pid) {
    for (int i = 0; i < scheduler.process_count; i++) {
        if (scheduler.processes[i].pid == pid) {
            scheduler.processes[i].state = PROCESS_READY;
            break;
        }
    }
}

process_t* get_current_process(void) {
    if (scheduler.current_process < scheduler.process_count) {
        return &scheduler.processes[scheduler.current_process];
    }
    return NULL;
}

process_t* get_process_by_pid(uint32_t pid) {
    for (int i = 0; i < scheduler.process_count; i++) {
        if (scheduler.processes[i].pid == pid) {
            return &scheduler.processes[i];
        }
    }
    return NULL;
}

void change_process() {
    if (initialized != true) {
        serial_printf("Scheduler not initialized!\n");
        return;
    }
    if (scheduler.process_count <= 1) {
        return;
    }
    int current = scheduler.current_process;
    int next = (current + 1) % scheduler.process_count;
    while (scheduler.processes[next].state != PROCESS_READY && next != current) {
        next = (next + 1) % scheduler.process_count;
    }
    if (next != current) {
        scheduler.processes[current].state = PROCESS_READY;
        scheduler.processes[next].state = PROCESS_RUNNING;
        scheduler.current_process = next;
        context_switch((uintptr_t)scheduler.processes[current].stack_ptr, (uintptr_t)scheduler.processes[next].stack_ptr);
    }
}