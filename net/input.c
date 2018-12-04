#include "ns.h"

extern union Nsipc nsipcbuf;

void
input(envid_t ns_envid)
{
	binaryname = "ns_input";

	// 	- read a packet from the device driver
	//	- send it to the network server
	// Hint: When you IPC a page to the network server, it will be
	// reading from it for a while, so don't immediately receive
	// another packet in to the same physical page.
	////////////////////MAGENDANZ//////////////////////
	char packet_data[MAX_PACKET_SIZE];
	size_t packet_size;
	int r;
	while(true) {
		do {
			packet_size = MAX_PACKET_SIZE;
			// Get a packet.
		} while ((r = sys_receive_packet(&packet_data, &packet_size)) == -E_NO_AVAIL_PKT);
		if (r < 0) {
			panic("sys_receive_packet: %e", r);
		}

		if ((r = sys_page_alloc(0, &nsipcbuf, PTE_U|PTE_W|PTE_P)) < 0) {
			panic("sys_page_alloc: %e", r);
		}

		memcpy(nsipcbuf.pkt.jp_data, packet_data, packet_size);
		nsipcbuf.pkt.jp_len = packet_size;

		ipc_send(ns_envid, NSREQ_INPUT, &nsipcbuf, PTE_U|PTE_W|PTE_P);
	}	
	//////////////////////////////////////////////////
}
