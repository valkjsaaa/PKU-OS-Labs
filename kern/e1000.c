#include <kern/e1000.h>

// LAB 6: Your driver code here

int
e1000_attach(struct pci_func *pcif){
	// enable PCI function
	pci_func_enable(pcif);
	return 0;
}