#include <scheduler.h>
#include <ps2_keyboard.h>
#include <serial.h>

/* A tiny keyboard process that just loops and yields. It does nothing
   because the real keyboard IRQ still feeds the kernel buffer. This
   process exists so you can list/terminate it from the shell for fun. */
void keyboard_process(void) {
    while (1) {
        /* pretend to wait for input: yield so other processes run */
        asm volatile ("hlt");
    }
}
