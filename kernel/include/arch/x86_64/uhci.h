#ifndef UHCI_H
#define UHCI_H

#include <stdint.h>

#define UHCI_USBCMD       0x00  // Command register
#define UHCI_USBSTS       0x02  // Status register
#define UHCI_USBINTR      0x04  // Interrupt enable
#define UHCI_FRNUM        0x06  // Frame number
#define UHCI_FRBASEADDR   0x08  // Frame list base address
#define UHCI_SOFMOD       0x0C  // Start of Frame modify
#define UHCI_PORTSC1      0x10  // Port 1 status/control
#define UHCI_PORTSC2      0x12  // Port 2 status/control

#define UHCI_TD_TERMINATE     0x00000001
#define UHCI_TD_QH_POINTER    0x00000002
#define UHCI_TD_DEPTH_FIRST   0x00000004

#define UHCI_QH_TERMINATE     0x00000001

#define TD_CTRL_ACTIVE        (1 << 23)
#define TD_CTRL_ERROR         (1 << 20)
#define TD_CTRL_LOW_SPEED     (1 << 26)
#define TD_CTRL_SHORT_PACKET  (1 << 27)

#define PID_OUT               0xE1
#define PID_IN                0x69
#define PID_SETUP             0x2D

typedef struct {
    uint32_t link_ptr;       // Link to next TD/QH
    uint32_t ctrl_status;    // Status and control bits
    uint32_t token;          // PID, device, endpoint, toggle, length
    uint32_t buffer;         // Physical address of data buffer
} __attribute__((packed)) uhci_td_t;

typedef struct {
    uint32_t head_ptr;       // Link to next QH (or TD), or terminate
    uint32_t element_ptr;    // First TD in this queue
} __attribute__((packed)) uhci_qh_t;

void uhci_init();

void uhci_start_transfer(uhci_qh_t *qh);

void uhci_poll_transfer(uhci_td_t *td);

#endif // UHCI_H