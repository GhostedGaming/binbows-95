#include <sata.h>
#include <serial.h>

static int check_type(HBA_PORT *port) {
	uint32_t ssts = port->ssts;

	uint8_t ipm = (ssts >> 8) & 0x0F;
	uint8_t det = ssts & 0x0F;

	if (det != HBA_PORT_DET_PRESENT)
		return AHCI_DEV_NULL;
	if (ipm != HBA_PORT_IPM_ACTIVE)
		return AHCI_DEV_NULL;

	switch (port->sig) {
	case SATA_SIG_ATAPI:
		return AHCI_DEV_SATAPI;
	case SATA_SIG_SEMB:
		return AHCI_DEV_SEMB;
	case SATA_SIG_PM:
		return AHCI_DEV_PM;
	default:
		return AHCI_DEV_SATA;
	}
}


void probe_port(HBA_MEM *abar) {
    uint32_t pi = abar->pi;
    int i = 0;

    while (i < 32) {
        if (pi & 1) {
            int dt = check_type(&abar->ports[i]);
			if (dt == AHCI_DEV_SATA) {
				trace_ahci("SATA drive found at port %d\n", i);
			}
			else if (dt == AHCI_DEV_SATAPI) {
				trace_ahci("SATAPI drive found at port %d\n", i);
			}
			else if (dt == AHCI_DEV_SEMB) {
				trace_ahci("SEMB drive found at port %d\n", i);
			}
			else if (dt == AHCI_DEV_PM) {
				trace_ahci("PM drive found at port %d\n", i);
			}
			else {
				trace_ahci("No drive found at port %d\n", i);
			}
        }

        pi >>= 1;
        i++;
    }
}