#include <inc/error.h>
#include <inc/string.h>

#include <kern/e1000.h>
#include <kern/env.h>
#include <kern/pmap.h>
#include <kern/sched.h>

///////////////////////MAGENDANZ/////////////////////////
volatile uint32_t* e1000_mmio;
struct tx_desc packet_out_q[E1000_NUM_OUT_DESC] __attribute__ ((aligned (16)));
struct rv_desc packet_in_q[E1000_NUM_IN_DESC] __attribute__ ((aligned (16)));
char packet_out_buffer[E1000_NUM_OUT_DESC][PACKET_BUF_SIZE];
char packet_in_buffer[E1000_NUM_IN_DESC][PACKET_BUF_SIZE];

uint64_t e1000_get_mac();

int 
pci_e1000_attach(struct pci_func *pcif)
{
	pci_func_enable(pcif);
	e1000_mmio = mmio_map_region(pcif->reg_base[0], pcif->reg_size[0]);

	for (int i = 0; i < E1000_NUM_OUT_DESC; i++) {
		packet_out_q[i].addr = PADDR(&packet_out_buffer[i]);
		packet_out_q[i].status = E1000_TXD_STAT_DD;
		packet_out_q[i].cmd = E1000_TXD_CMD_RS;
	}

	e1000_mmio[E1000_TDBAL] = PADDR(packet_out_q);
	e1000_mmio[E1000_TDLEN] = E1000_NUM_OUT_DESC * sizeof(struct tx_desc);
	e1000_mmio[E1000_TDH] = 0;
	e1000_mmio[E1000_TDT] = 0;
	e1000_mmio[E1000_TCTL] |= E1000_TCTL_EN | E1000_TCTL_PSP | E1000_TCTL_CT_ETH | E1000_TCTL_COLD_FULL;
	e1000_mmio[E1000_TIPG] = E1000_TIPG_STANDARD;

	for (int i = 0; i < E1000_NUM_IN_DESC; i++) {
		packet_in_q[i].addr = PADDR(&packet_in_buffer[i]);
	}

	uint64_t mac = e1000_get_mac();
	cprintf("MAC: %llx\n", mac);
	e1000_mmio[E1000_RAL] = (uint32_t) mac; //0x12005452;
	e1000_mmio[E1000_RAH] = (uint32_t)(mac >> 32); //0x5634;
	e1000_mmio[E1000_RAH] |= E1000_RAH_AV;
	e1000_mmio[E1000_RDBAL] = PADDR(packet_in_q);
	e1000_mmio[E1000_RDLEN] = E1000_NUM_IN_DESC * sizeof(struct rv_desc);
	e1000_mmio[E1000_RDH] = 0;
	e1000_mmio[E1000_RDT] = E1000_NUM_IN_DESC - 1;
	e1000_mmio[E1000_IMS] |= E1000_ICR_RXT;
	e1000_mmio[E1000_RCTL] &= E1000_RCTL_LBM_NO;
	e1000_mmio[E1000_RCTL] &= E1000_RCTL_BSIZE_2048;
	e1000_mmio[E1000_RCTL] |= E1000_RCTL_SECRC;
	e1000_mmio[E1000_RCTL] &= E1000_RCTL_LPE_NO;
	e1000_mmio[E1000_RCTL] |= E1000_RCTL_EN;

	return 1;
}

void 
e1000_clear_interrupt()
{
	e1000_mmio[E1000_ICR] |= E1000_ICR_RXT;
}

// Transmits a packet containing the data at packet_data.
// On completion, packet_size contains the number of bytes
// copied to the E1000 for sending.
int
e1000_transmit_packet(void* packet_data, size_t* packet_size)
{
	size_t tdt = e1000_mmio[E1000_TDT];

	// Throw error back to user if network card is full.
	if (!(packet_out_q[tdt].status & E1000_TXD_STAT_DD)) {
		*packet_size = 0;
		cprintf("Network card full.");
		return -E_NO_NET_MEM;
	}
	// If packet is larger than max size, truncate packet.
	*packet_size = MAX_PACKET_SIZE > *packet_size ? *packet_size : MAX_PACKET_SIZE;
	memcpy(&packet_out_buffer[tdt], packet_data, *packet_size);

	packet_out_q[tdt].length = *packet_size;
	packet_out_q[tdt].cmd |= E1000_TXD_CMD_EOP;
	packet_out_q[tdt].status &= ~E1000_TXD_STAT_DD;
	// Circular array increment index.
	e1000_mmio[E1000_TDT] = (tdt + 1) % E1000_NUM_OUT_DESC;

	return 0;
}

// Receive the next packet in the e1000 network card. If no
// packet available, blocks until a packet arrives, then returns
// with -E-E1000_RECEIVE_EMPTY. On return, packet_size contains 
// the number of bytes copied from the E1000.
int
e1000_receive_packet(void* packet_data, size_t* packet_size)
{
	// Increment circular array tail.
	size_t rdt = (e1000_mmio[E1000_RDT] + 1) % E1000_NUM_IN_DESC;

	// No packets to receive.
	if (!(packet_in_q[rdt].status & E1000_RXD_STAT_DD)) {
		*packet_size = 0;
		return -E_NO_AVAIL_PKT;
	}

	// If packet is larger than buffer, truncate packet.
	*packet_size = packet_in_q[rdt].length > *packet_size ? *packet_size : packet_in_q[rdt].length;
	memcpy(packet_data, &packet_in_buffer[rdt], *packet_size);
	packet_in_q[rdt].status &= ~E1000_RXD_STAT_DD;

	// Only increment size once done with descriptor.
	e1000_mmio[E1000_RDT] = rdt;
	return 0;

}

// Returns the 48-bit MAC as the first 48 bits of a uint64_t.
uint64_t
e1000_get_mac()
{
	uint64_t mac = 0;
	volatile uint32_t* eerd = &e1000_mmio[E1000_EERD];
	for (int word = 0; word < E1000_EEPROM_MAC_WORDS; word++) {
		// Set EEPROM register to read word of MAC.
		*eerd |= (word << E1000_EERD_ADDR) | E1000_EERD_READ;
		// Wait until fetch done.
		while (!(*eerd & E1000_EERD_DONE));

		uint16_t word_bytes = *eerd >> E1000_EERD_DATA;
		mac |= ((uint64_t)word_bytes) << (word*16);

		// Reset register to stop reads.
		*eerd = 0x0;
	}
	return mac;
}
////////////////////////////////////////////////////////

