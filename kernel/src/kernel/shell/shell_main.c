#include <shell.h>
#include <scheduler.h>
#include <ps2_keyboard.h>
#include <util.h>
#include <framebuffer.h>
#include <serial.h>

void shell_main() {
    serial_printf("[DEBUG] Shell started!\n");
    shell_init();
    while (1) {
        serial_printf("[DEBUG] Shell loop\n");
        char received = wait_for_input();
        char* cmd = input(received);
        if (cmd) parse_command();
        yield();
    }
}

