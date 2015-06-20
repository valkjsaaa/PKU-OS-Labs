#include "ns.h"

extern union Nsipc nsipcbuf;

void
output(envid_t ns_envid)
{
	binaryname = "ns_output";

	// LAB 6: Your code here:
	// 	- read a packet from the network server
	//	- send the packet to the device driver
	uint32_t req;

	while (1) {
		req = ipc_recv(0, &nsipcbuf, 0);
		if (req == NSREQ_OUTPUT) {
			while(sys_net_xmit(nsipcbuf.pkt.jp_data, nsipcbuf.pkt.jp_len) < 0) {
				sys_yield();
			}
		}
	}
}
