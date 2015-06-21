#include <kern/e1000.h>

// LAB 6: Your driver code here

struct e1000_tx_desc tx_desc_buf[TXRING_LEN] __attribute__ ((aligned (PGSIZE)));
struct e1000_data tx_data_buf[TXRING_LEN] __attribute__ ((aligned (PGSIZE)));

struct e1000_rx_desc rx_desc_buf[RXRING_LEN] __attribute__ ((aligned (PGSIZE)));
struct e1000_data rx_data_buf[RXRING_LEN] __attribute__ ((aligned (PGSIZE)));

static void
init_desc(){
	int i;
	for (i = 0; i < TXRING_LEN; ++i)
	{
		tx_desc_buf[i].buffer_addr = PADDR(&tx_data_buf[i]);
		tx_desc_buf[i].upper.fields.status = E1000_TXD_STAT_DD;
	}
	for (i = 0; i < RXRING_LEN; ++i)
	{
		rx_desc_buf[i].buffer_addr = PADDR(&rx_data_buf[i]);
	}
}

static void
e1000_init(){
	assert(e1000[E1000_STATUS] == 0x80080783);
	e1000[E1000_TDBAL] = PADDR(tx_desc_buf);
	e1000[E1000_TDBAH] = 0x0;
	e1000[E1000_TDH] = 0x0;
	e1000[E1000_TDT] = 0x0;
	e1000[E1000_TDLEN] = TXRING_LEN * sizeof(struct e1000_tx_desc);
	e1000[E1000_TCTL] = VALUEATMASK(1, E1000_TCTL_EN) |
						VALUEATMASK(1, E1000_TCTL_PSP) |
						VALUEATMASK(0x10, E1000_TCTL_CT) |
						VALUEATMASK(0x40, E1000_TCTL_COLD);
	e1000[E1000_TIPG] = VALUEATMASK(10, E1000_TIPG_IPGT) |
						VALUEATMASK(8, E1000_TIPG_IPGR1) |
						VALUEATMASK(6, E1000_TIPG_IPGR2);

	// LAB 6: Challenge: read MAC Address from EEPROM
	e1000[E1000_EERD] = 0x0 << E1000_EEPROM_RW_ADDR_SHIFT;
	e1000[E1000_EERD] |= E1000_EEPROM_RW_REG_START;
	while (!(e1000[E1000_EERD] & E1000_EEPROM_RW_REG_DONE));
	e1000[E1000_RAL] = e1000[E1000_EERD] >> 16;

	e1000[E1000_EERD] = 0x1 << E1000_EEPROM_RW_ADDR_SHIFT;
	e1000[E1000_EERD] |= E1000_EEPROM_RW_REG_START;
	while (!(e1000[E1000_EERD] & E1000_EEPROM_RW_REG_DONE));
	e1000[E1000_RAL] |= e1000[E1000_EERD] & 0xffff0000;

	e1000[E1000_EERD] = 0x2 << E1000_EEPROM_RW_ADDR_SHIFT;
	e1000[E1000_EERD] |= E1000_EEPROM_RW_REG_START;
	while (!(e1000[E1000_EERD] & E1000_EEPROM_RW_REG_DONE));
	e1000[E1000_RAH] = e1000[E1000_EERD] >> 16;

	e1000[E1000_RAH] |= E1000_RAH_AV;

	e1000[E1000_RDBAL] = PADDR(rx_desc_buf);
	e1000[E1000_RDBAH] = 0x0;
	e1000[E1000_RDLEN] = RXRING_LEN * sizeof(struct e1000_rx_desc);
	e1000[E1000_RDH] = 0x0;
	e1000[E1000_RDT] = RXRING_LEN;
	e1000[E1000_RCTL] = E1000_RCTL_EN |
						!E1000_RCTL_LPE |
						E1000_RCTL_LBM_NO |
						E1000_RCTL_RDMTS_HALF |
						E1000_RCTL_MO_0 |
						E1000_RCTL_BAM |
						E1000_RCTL_BSEX |
						E1000_RCTL_SZ_4096 |
						E1000_RCTL_SECRC;
}

int
e1000_attach(struct pci_func *pcif){
	// enable PCI function
	pci_func_enable(pcif);
	// init descriptor
	init_desc();
	// create virtual memory mapping
	e1000 = mmio_map_region(pcif->reg_base[0], pcif->reg_size[0]);
	// check the status register
	assert(e1000[E1000_STATUS] == 0x80080783);
	// init the hardware
	e1000_init();
	return 0;
}

int
e1000_xmit(uint8_t * addr, size_t length){
	uint32_t tail = e1000[E1000_TDT];
	struct e1000_tx_desc * tail_desc = &tx_desc_buf[tail];
	if (tail_desc->upper.fields.status != E1000_TXD_STAT_DD)
	{
		return -1;
	}
	length = length > DATA_SIZE ? DATA_SIZE : length;
	memmove(&tx_data_buf[tail], addr, length);
	tail_desc->lower.flags.length = length;
	tail_desc->upper.fields.status = 0;
	tail_desc->lower.data |=  (E1000_TXD_CMD_RS |
							   E1000_TXD_CMD_EOP);
	e1000[E1000_TDT] = (tail + 1) % TXRING_LEN;
	return 0;
}

int
e1000_recv(uint8_t * data){
	static uint32_t real_tail = 0;
	uint32_t tail = real_tail;
	struct e1000_rx_desc * tail_desc = &rx_desc_buf[tail];
	if (!(tail_desc->status & E1000_RXD_STAT_DD))
	{
		return -1;
	}
	size_t length = tail_desc->length;
	memmove(data, &rx_data_buf[tail], length);
	tail_desc->status = 0;
	e1000[E1000_RDT] = tail;
	real_tail = (tail + 1) % RXRING_LEN;
	return length;
}

void read_mac_address(uint8_t* mac_address){
	*(uint32_t*)mac_address = (uint32_t)e1000[E1000_RAL];
	*(uint16_t*)(mac_address + 4) = (uint16_t)e1000[E1000_RAH];
}