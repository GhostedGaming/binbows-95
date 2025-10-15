#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdbool.h>
#include <stdint.h>

#define MAX_PROCESS_COUNT 255

extern volatile bool scheduler_tick;

typedef enum {
    PROCESS_READY,
    PROCESS_RUNNING,
    PROCESS_BLOCKED,
    PROCESS_TERMINATED
} process_state_t;

typedef struct {
    uint32_t pid;
    process_state_t state;
    uint32_t priority;
    uint64_t* stack_ptr;
    uint64_t* stack_base;
    uint64_t pml4_phys; // physical address of this process's PML4
    bool is_user;
    uint64_t user_stack_vaddr;
    uint32_t time_slice;
    uint32_t cpu_time_used;
    void (*entry_point)(void);
} process_t;

typedef struct {
    process_t processes[MAX_PROCESS_COUNT];
    uint32_t process_count;
    uint32_t current_process;
    uint32_t next_pid;
} scheduler_t;

void scheduler_init(void);
void context_switch(uint64_t** save_rsp_location, uint64_t** load_rsp_location, uint64_t usermode);
int create_process(void (*entry_point)(void), uint32_t priority);
void terminate_process(uint32_t pid);
void change_process(void);

void block_process(uint32_t pid);
void unblock_process(uint32_t pid);
process_t* get_current_process(void);
process_t* get_process_by_pid(uint32_t pid);

uint32_t get_process_count(void);
process_t* get_process_at(uint32_t index);

void free_process_stack(uint64_t* stack_base);
void setup_initial_stack(process_t* process);
void yield(void);

void test_scheduler(void);

#endif