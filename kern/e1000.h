#ifndef JOS_KERN_E1000_H
#define JOS_KERN_E1000_H

//////////////////MAGENDANZ////////////////////
#include <inc/types.h>
#include <kern/pci.h>

#define E1000_VENDOR_ID		0x8086
#define E1000_DEV_ID_82540EM	0x100E

// Descriptor MMIO register uint32_t indexes
#define E1000_TDBAL		0x03800 >> 2  /* TX Descriptor Base Address Low - RW */
#define E1000_TDLEN		0x03808 >> 2  /* TX Descriptor Length - RW */
#define E1000_TDH		0x03810 >> 2  /* TX Descriptor Head - RW */
#define E1000_TDT		0x03818 >> 2  /* TX Descripotr Tail - RW */
#define E1000_TCTL		0x00400 >> 2  /* TX Control - RW */
#define E1000_TIPG		0x00410 >> 2  /* TX Inter-packet gap -RW */

// Transmit control flags
#define E1000_TCTL_EN     	0x00000002    /* enable tx */
#define E1000_TCTL_PSP		0x00000008    /* pad short packets */
#define E1000_TCTL_CT		0x00000ff0    /* collision threshold */
#define E1000_TCTL_CT_ETH	0x00000100    /* ethernet standard for TCTL.CT */
#define E1000_TCTL_COLD		0x003ff000    /* collision distance */
#define E1000_TCTL_COLD_FULL	0x00040000    /* full duplex operation for TCTL.COLD */

// Status transmit descriptor flags
#define E1000_TXD_CMD_RS     	0x08 /* Report Status */
#define E1000_TXD_STAT_DD   	0x01 /* Descriptor Done */	

#define E1000_TIPG_STANDARD	10
#define E1000_NUM_QUEUE_DESC	32

struct tx_desc
{
	uint64_t addr;
	uint16_t length;
	uint8_t cso;
	uint8_t cmd;
	uint8_t status;
	uint8_t css;
	uint16_t special;
};

int pci_e1000_attach(struct pci_func *pcif);
int e1000_transmit_packet(char* packet_data, size_t packet_size);
//////////////////////////////////////////////

#endif  // SOL >= 6
