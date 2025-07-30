#include <limine.h>
#include <stdbool.h>
#include <kernel.h>

__attribute__((used, section(".limine_requests_start")))
static volatile LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests")))
static volatile LIMINE_BASE_REVISION(3);

__attribute__((used, section(".limine_requests_end")))
static volatile LIMINE_REQUESTS_END_MARKER;

void kmain(void) {
    // Check base revision support
    if (LIMINE_BASE_REVISION_SUPPORTED == false) {
        // Handle error - halt or panic
        __asm__("cli");
        for (;;) {
            __asm__("hlt");
        }
    }
    
    kernel_main();
}