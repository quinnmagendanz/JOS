#include <inc/error.h>
#include <inc/string.h>

#include <kern/e1000.h>
#include <kern/env.h>
#include <kern/pmap.h>

///////////////////////MAGENDANZ/////////////////////////
volatile uint32_t* e1000_mmio;
struct tx_desc packet_out_q[E1000_NUM_OUT_DESC];
struct rv_desc packet_in_q[E1000_NUM_IN_DESC];
char packet_out_buffer[E1000_NUM_OUT_DESC][MAX_PACKET_SIZE];
char packet_in_buffer[E1000_NUM_IN_DESC][IN_PACKET_BUFF_SIZE];

int 
pci_e1000_attach(struct pci_func *pcif)
{
	pci_func_enable(pcif);
	e1000_mmio = mmio_map_region(pcif->reg_base[0], pcif->reg_size[0]);

	e1000_mmio[E1000_TDBAL] = PADDR(&packet_out_q);
	e1000_mmio[E1000_TDLEN] = sizeof(packet_out_q);
	e1000_mmio[E1000_TDH] = 0;
	e1000_mmio[E1000_TDT] = 0;
	e1000_mmio[E1000_TCTL] |= E1000_TCTL_EN | E1000_TCTL_PSP | E1000_TCTL_CT_ETH | E1000_TCTL_COLD_FULL;
	e1000_mmio[E1000_TIPG] = E1000_TIPG_STANDARD;

	for (int i = 0; i < E1000_NUM_IN_DESC; i++) {
		packet_in_q[i].addr = PADDR(&packet_in_buffer[i]);
	}

	e1000_mmio[E1000_RAL] = 0x52540012;
	e1000_mmio[E1000_RAH] = (uint16_t)0x3456;
	e1000_mmio[E1000_RDBAL] = PADDR(&packet_in_q);
	e1000_mmio[E1000_RDLEN] = sizeof(packet_in_q);
	e1000_mmio[E1000_RCTL] |= E1000_RCTL_EN | E1000_RCTL_SZ_2048;	

	return 1;
}

int
e1000_transmit_packet(void* packet_data, size_t* packet_size)
{
	volatile uint32_t* tdt = &e1000_mmio[E1000_TDT];

	// Throw error back to user if network card is full.
	if ((packet_out_q[*tdt].cmd & E1000_TXD_CMD_RS) == E1000_TXD_CMD_RS && 
		(packet_out_q[*tdt].status & E1000_TXD_STAT_DD) != E1000_TXD_STAT_DD) {
		return -E_NO_NET_MEM;
	}

	*packet_size = MAX_PACKET_SIZE > *packet_size ? *packet_size : MAX_PACKET_SIZE;
	memcpy(&packet_out_buffer[*tdt], packet_data, *packet_size);

	packet_out_q[*tdt].addr = PADDR(&packet_out_buffer[*tdt]);
	packet_out_q[*tdt].length = *packet_size;
	packet_out_q[*tdt].cmd = E1000_TXD_CMD_RS | E1000_TXD_CMD_EOP;
	packet_out_q[*tdt].cso = 0;
	packet_out_q[*tdt].status = 0;
	packet_out_q[*tdt].css = 0;
	packet_out_q[*tdt].special = 0;
	// Circular array increment index.
	*tdt = (*tdt + 1) % E1000_NUM_OUT_DESC;

	return 0;
}

int
e1000_receive_packet(void* packet_data, size_t* packet_size)
{
	volatile uint32_t* rdt = &e1000_mmio[E1000_RDT];

	// No packets to receive.
	if ((packet_in_q[*rdt].status & E1000_RXD_STAT_DD) == E1000_RXD_STAT_DD) {
		*packet_size = 0;
		return 0;
	}

	*packet_size = IN_PACKET_BUFF_SIZE > *packet_size ? *packet_size : IN_PACKET_BUFF_SIZE;
	memcpy(packet_data, &packet_in_buffer[*rdt], *packet_size);

	*rdt = (*rdt + 1) % E1000_NUM_IN_DESC;
	return 0;
}
////////////////////////////////////////////////////////

