#ifndef JOS_KERN_E1000_H
#define JOS_KERN_E1000_H

#include <kern/pci.h>
#include <kern/pmap.h>

// Some macro adapted from qemu e1000_hw.h

/* PCI Device IDs */
#define E1000_DEV_ID_82540EM             0x100E
/* Register Set. (82543, 82544)
 *
 * Registers are defined to be 32 bits and  should be accessed as 32 bit values.
 * These registers are physically located on the NIC, but are mapped into the
 * host memory address space.
 *
 * RW - register is both readable and writable
 * RO - register is read only
 * WO - register is write only
 * R/clr - register is read only and is cleared when read
 * A - register array
 */
#define E1000_STATUS   0x00008/4  /* Device Status - RO */

volatile uint32_t * e1000;

int e1000_attach(struct pci_func *pcif);

#endif	// JOS_KERN_E1000_H
