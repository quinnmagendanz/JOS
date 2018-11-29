#include <inc/error.h>

#include <kern/e1000.h>
#include <kern/pmap.h>

///////////////////////MAGENDANZ/////////////////////////
volatile uint32_t* e1000_mmio;
struct tx_desc packet_io_q[E1000_NUM_QUEUE_DESC];

int 
pci_e1000_attach(struct pci_func *pcif)
{
	pci_func_enable(pcif);
	e1000_mmio = mmio_map_region(pcif->reg_base[0], pcif->reg_size[0]);

	e1000_mmio[E1000_TDBAL] = PADDR(&packet_io_q);
	e1000_mmio[E1000_TDLEN] = sizeof(packet_io_q);
	e1000_mmio[E1000_TDH] = 0;
	e1000_mmio[E1000_TDT] = 0;
	e1000_mmio[E1000_TCTL] |= E1000_TCTL_EN | E1000_TCTL_PSP | E1000_TCTL_CT_ETH | E1000_TCTL_COLD_FULL;
	e1000_mmio[E1000_TIPG] = E1000_TIPG_STANDARD;

	return 1;
}

int
e1000_transmit_packet(char* packet_data, size_t packet_size)
{
	volatile uint32_t* tdt = &e1000_mmio[E1000_TDT];
	// Throw error back to user if network card is full.
	if ((packet_io_q[*tdt].status & E1000_TXD_STAT_DD) != E1000_TXD_STAT_DD) {
		return -E_NO_NET_MEM;
	}
	packet_io_q[*tdt].addr = (uint32_t)packet_data;
	packet_io_q[*tdt].length = packet_size;
	packet_io_q[*tdt].cmd = E1000_TXD_CMD_RS;
	packet_io_q[*tdt].cso = 0;
	packet_io_q[*tdt].status = 0;
	packet_io_q[*tdt].css = 0;
	packet_io_q[*tdt].special = 0;
	// Circular array increment index.
	*tdt = (*tdt + 1) % E1000_NUM_QUEUE_DESC;
	return 0;
}
////////////////////////////////////////////////////////

