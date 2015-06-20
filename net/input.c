#include "ns.h"

extern union Nsipc nsipcbuf;

void
input(envid_t ns_envid)
{
	binaryname = "ns_input";

	// LAB 6: Your code here:
	// 	- read a packet from the device driver
	//	- send it to the network server
	// Hint: When you IPC a page to the network server, it will be
	// reading from it for a while, so don't immediately receive
	// another packet in to the same physical page.

	while(1) {
	    while(sys_page_alloc(0, &nsipcbuf, PTE_U | PTE_P | PTE_W) < 0);
	    while((nsipcbuf.pkt.jp_len = sys_net_recv(nsipcbuf.pkt.jp_data)) < 0) {
	    		// cprintf("haha\n");
				sys_yield();
		}
		while (sys_ipc_try_send(ns_envid, NSREQ_INPUT, &nsipcbuf, PTE_U | PTE_P | PTE_W) < 0);
	}
}
