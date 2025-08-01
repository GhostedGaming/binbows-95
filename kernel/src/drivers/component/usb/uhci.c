#include <stdint.h>
#include <pci.h>
#include <io.h>
#include <serial.h>
#include <mem.h>
#include <uhci.h>

#define FRAME_LIST_SIZE 1024
#define FRAME_LIST_ENTRY_TERMINATE 0x00000001
#define HHDM_OFFSET 0xffff800000000000ULL

// Very Rizzy USB Stuff
void uhci_init() {
    void *frame_list = page_alloc();
    if (!frame_list) {
        serial_printf("ERROR: Failed to allocate frame list memory!\n");
        return;
    }

    // Ensure 4KB alignment for frame list
    if (((uintptr_t)frame_list & 0xFFF) != 0) {
        serial_printf("ERROR: Frame list not 4KB aligned! Got %p\n", frame_list);
        return;
    }

    // Terminate all 1024 frame list entries
    uint32_t *fl = (uint32_t *)frame_list;
    for (int i = 0; i < FRAME_LIST_SIZE; i++) {
        fl[i] = FRAME_LIST_ENTRY_TERMINATE;
    }
    serial_printf("Allocated and initialized frame list at virtual %p\n", frame_list);

    serial_printf("Starting UHCI controller scan...\n");

    for (uint8_t bus = 0; bus < 4; ++bus) {
        serial_printf("Scanning bus %02x\n", bus);

        for (uint8_t device = 0; device < 32; ++device) {
            for (uint8_t function = 0; function < 8; ++function) {
                uint16_t vendor = pci_config_read_word(bus, device, function, 0x00);
                if (vendor == 0xFFFF) {
                    if (function == 0)
                        serial_printf("  %02x:%02x.0 - No device\n", bus, device);
                    continue;
                }

                uint16_t device_id = pci_config_read_word(bus, device, function, 0x02);
                uint8_t class_code = pci_config_read_byte(bus, device, function, 0x0B);
                uint8_t subclass   = pci_config_read_byte(bus, device, function, 0x0A);
                uint8_t prog_if    = pci_config_read_byte(bus, device, function, 0x09);

                serial_printf("  %02x:%02x.%x - Vendor: 0x%04x, Device: 0x%04x, Class: 0x%02x, Subclass: 0x%02x, ProgIF: 0x%02x\n",
                              bus, device, function, vendor, device_id, class_code, subclass, prog_if);

                // Check if this is a UHCI controller
                if (class_code == 0x0C && subclass == 0x03 && prog_if == 0x00) {
                    serial_printf("*** Found UHCI controller at %02x:%02x.%x ***\n", bus, device, function);

                    // Print IRQ line/pin
                    uint8_t irq_line = pci_config_read_byte(bus, device, function, 0x3C);
                    uint8_t irq_pin  = pci_config_read_byte(bus, device, function, 0x3D);
                    serial_printf("IRQ Line: %u, IRQ Pin: %u\n", irq_line, irq_pin);

                    // Enable I/O and bus mastering
                    uint16_t command = pci_config_read_word(bus, device, function, 0x04);
                    serial_printf("Current PCI command register: 0x%04x\n", command);
                    command |= 0x0005;
                    pci_config_write_word(bus, device, function, 0x04, command);
                    serial_printf("Enabled I/O and bus mastering (command=0x%04x)\n", command);

                    // Read BAR4 (I/O base address)
                    uint32_t bar = pci_config_read_dword(bus, device, function, 0x20);
                    serial_printf("BAR4 value: 0x%08x\n", bar);

                    if ((bar & 0x01) == 0) {
                        serial_printf("ERROR: BAR4 is not I/O space! Skipping controller...\n");
                        continue;
                    }

                    uint16_t io_base = bar & 0xFFF0;
                    serial_printf("UHCI I/O Base: 0x%04x\n", io_base);

                    serial_printf("Initializing UHCI controller...\n");

                    // Controller Reset with timeout
                    outw(io_base + 0x00, 0x0004);
                    int timeout = 100000;
                    while ((inw(io_base + 0x00) & 0x0004) && --timeout);
                    if (timeout == 0) {
                        serial_printf("ERROR: Controller reset timed out!\n");
                        return;
                    }
                    serial_printf("Controller reset complete\n");

                    // Clear status
                    outw(io_base + 0x02, 0x3F);
                    serial_printf("Status register cleared\n");

                    // Disable interrupts
                    outw(io_base + 0x04, 0x0000);
                    serial_printf("Interrupts disabled\n");

                    // Set frame number and SOF modify to 0
                    outw(io_base + 0x06, 0x0000);
                    outb(io_base + 0x0C, 0x00);
                    serial_printf("Frame number and SOF modify set to 0\n");

                    // Set Frame List Base Address
                    uint32_t phys_frame_list = (uint32_t)((uintptr_t)frame_list - HHDM_OFFSET);
                    outl(io_base + 0x08, phys_frame_list);
                    serial_printf("Frame List Base Address set to physical 0x%08x (virtual %p)\n", phys_frame_list, frame_list);

                    // Set Configure Flag
                    outw(io_base + 0x0C, 0x0001);
                    serial_printf("Configure Flag set\n");

                    // Set Run bit (start controller)
                    outw(io_base + 0x00, 0x0001);
                    serial_printf("Controller started (Run bit set)\n");

                    // Root port reset + enable
                    for (int port = 0; port < 2; port++) {
                        uint16_t portsc = inw(io_base + 0x10 + (port * 2));

                        // Reset
                        outw(io_base + 0x10 + (port * 2), portsc | (1 << 9));
                        for (volatile int i = 0; i < 100000; ++i); // Delay

                        // Clear Reset
                        portsc = inw(io_base + 0x10 + (port * 2));
                        outw(io_base + 0x10 + (port * 2), portsc & ~(1 << 9));

                        // Enable port
                        portsc = inw(io_base + 0x10 + (port * 2));
                        outw(io_base + 0x10 + (port * 2), portsc | (1 << 2));

                        serial_printf("Port %d reset and enabled\n", port);
                    }

                    serial_printf("UHCI initialization complete!\n");
                    return;
                }
            }
        }
    }

    serial_printf("No usable UHCI controller found after scanning all buses!\n");
}

void uhci_write() {
    uint8_t *buf = page_alloc();
    buf[0] = 0x00;
    uint32_t phys_buf = (uint32_t)((uintptr_t)buf - HHDM_OFFSET);

    uhci_td_t *td = page_alloc();
    uint32_t phys_td = (uint32_t)((uintptr_t)td - HHDM_OFFSET);
    td->link_ptr = UHCI_TD_TERMINATE;
    td->ctrl_status = (3 << 27) | (1 << 23);
    td->token = (0x7 << 21) | (0 << 19) | (0 << 15) | (1 << 8) | 0xE1;
    td->buffer = phys_buf;

    uhci_qh_t *qh = page_alloc();
    uint32_t phys_qh = (uint32_t)((uintptr_t)qh - HHDM_OFFSET);
    qh->head_ptr = UHCI_QH_TERMINATE;
    qh->element_ptr = phys_td;

    extern uint32_t *frame_list;
    frame_list[0] = phys_qh | 0x2;

    while (td->ctrl_status & (1 << 23));

    if (td->ctrl_status & 0x1F) {
        serial_printf("UHCI write failed: status=0x%08x\n", td->ctrl_status);
    } else {
        serial_printf("UHCI write successful!\n");
    }
}
