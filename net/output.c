#include "ns.h"

extern union Nsipc nsipcbuf;

void
output(envid_t ns_envid)
{
	binaryname = "ns_output";

	// 	- read a packet from the network server
	//	- send the packet to the device driver
	/////////////////////MAGENDANZ///////////////////////
	int r;
	while(true) {
		r = sys_ipc_recv(&nsipcbuf);
		do {
			r = sys_transmit_packet(nsipcbuf.pkt.jp_data, (size_t*)&nsipcbuf.pkt.jp_len);
			if (r < 0 && r != -E_NO_NET_MEM) {
				cprintf("sys_transmit_packet: %e\n", r);
				return;
			}
			sys_yield();
		} while (r == -E_NO_NET_MEM);
	}
	////////////////////////////////////////////////////
}
