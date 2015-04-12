// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/x86.h>

#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/kdebug.h>
#include <kern/trap.h>

#include <kern/pmap.h>

#define CMDBUF_SIZE	80	// enough for one VGA text line


struct Command {
	const char *name;
	const char *desc;
	// return -1 to force monitor to exit
	int (*func)(int argc, char** argv, struct Trapframe* tf);
};

static struct Command commands[] = {
	{ "help", "Display this list of commands", mon_help },
	{ "kerninfo", "Display information about the kernel", mon_kerninfo },
	{ "backtrace", "Back trace the functions", mon_backtrace},
	{ "showmappings", "Show virtual memory mappings and permission", mon_showmappings},
	{ "setperm", "Set virtual memory permission", mon_setperm},
	{ "dumpva", "Dump a range of virtual memory", mon_dumpva},
	{ "dumppa", "Dump a range of physical memory", mon_dumppa}
};
#define NCOMMANDS (sizeof(commands)/sizeof(commands[0]))

/***** Implementations of basic kernel monitor commands *****/

int
mon_help(int argc, char **argv, struct Trapframe *tf)
{
	int i;

	for (i = 0; i < NCOMMANDS; i++)
		cprintf("%s - %s\n", commands[i].name, commands[i].desc);
	return 0;
}

int
mon_kerninfo(int argc, char **argv, struct Trapframe *tf)
{
	extern char _start[], entry[], etext[], edata[], end[];

	cprintf("Special kernel symbols:\n");
	cprintf("  _start                  %08x (phys)\n", _start);
	cprintf("  entry  %08x (virt)  %08x (phys)\n", entry, entry - KERNBASE);
	cprintf("  etext  %08x (virt)  %08x (phys)\n", etext, etext - KERNBASE);
	cprintf("  edata  %08x (virt)  %08x (phys)\n", edata, edata - KERNBASE);
	cprintf("  end    %08x (virt)  %08x (phys)\n", end, end - KERNBASE);
	cprintf("Kernel executable memory footprint: %dKB\n",
		ROUNDUP(end - entry, 1024) / 1024);
	return 0;
}

int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
	uint32_t *ebp = (uint32_t*)read_ebp();
	uint32_t eip = ebp[1];
	struct Eipdebuginfo info;

	cprintf ("Stack backtrace:\n");
	while (ebp){
		cprintf ("  ebp %08x  eip %08x  args  ", ebp, eip);

		int i = 2;
		for (i; i < 7; ++i)
		{
			cprintf("%08x ", ebp[i]);
		}
		cprintf("\n");

		debuginfo_eip(eip,&info);
		cprintf ("         %s:%d: %.*s+%d\n",
				 info.eip_file,
				 info.eip_line,
				 info.eip_fn_namelen,
				 info.eip_fn_name,
				 ebp[1]-info.eip_fn_addr);

		ebp = (uint32_t*) ebp[0];
		eip = ebp[1];
	}
	return 0;
}

/***** Implementations page debug commands *****/

int
internal_showmappings(void* addr_start, void* addr_end)
{
	for (addr_start; addr_start < addr_end; addr_start+=PGSIZE)
	{
		physaddr_t physaddr_ptr = *(pgdir_walk(kern_pgdir, addr_start, 0));
		if(physaddr_ptr) {
			cprintf("va %08x pa %08x perm %08x\n", addr_start, PTE_ADDR(physaddr_ptr), physaddr_ptr & 0xFFF);
		}
		else {
			cprintf("va %08x page not allcated", addr_start);
		}
	}
	return 0;
}

int
mon_showmappings(int argc, char **argv, struct Trapframe *tf)
{
	void* addr_start = (void*)strtol(argv[1], NULL, 0);
	void* addr_end = (void*)strtol(argv[2], NULL, 0);
	return internal_showmappings(addr_start, addr_end);
}

int
mon_setperm(int argc, char **argv, struct Trapframe *tf)
{
    void* addr = (void*)strtol(argv[1], NULL, 0);
    pte_t *pte = pgdir_walk(kern_pgdir, (void *)addr, 0);
    uint32_t perm = 0;
    if (strchr(argv[2], 'P')) perm |= PTE_P;
    if (strchr(argv[2], 'W')) perm |= PTE_W;
    if (strchr(argv[2], 'U')) perm |= PTE_U;
    if (strchr(argv[2], '0')) perm = 0;
    *pte = *pte | perm;
    internal_showmappings(addr, addr+PGSIZE);
    return 0;
}


int
mon_dumpva(int argc, char **argv, struct Trapframe *tf)
{
	void* addr_start = (void*)strtol(argv[1], NULL, 0);
	void* addr_end = (void*)strtol(argv[2], NULL, 0);

	cprintf("Raw dump from virtual address from %08x to %08x\n", addr_start, addr_end);

	for (addr_start; addr_start < addr_end; ++addr_start)
	{
		cprintf("%08x", *(char*)addr_start);
	}
	cprintf("\n");
	return 0;
}

int
mon_dumppa(int argc, char **argv, struct Trapframe *tf)
{
	physaddr_t addr_start = strtol(argv[1], NULL, 0);
	physaddr_t addr_end = strtol(argv[2], NULL, 0);

	cprintf("Raw dump from virtual address from %08x to %08x\n", addr_start, addr_end);

	for (addr_start; addr_start < addr_end; ++addr_start)
	{
		cprintf("%08x", *(char*)KADDR(addr_start));
	}
	cprintf("\n");
	return 0;
}

/***** Kernel monitor command interpreter *****/

#define WHITESPACE "\t\r\n "
#define MAXARGS 16

static int
runcmd(char *buf, struct Trapframe *tf)
{
	int argc;
	char *argv[MAXARGS];
	int i;

	// Parse the command buffer into whitespace-separated arguments
	argc = 0;
	argv[argc] = 0;
	while (1) {
		// gobble whitespace
		while (*buf && strchr(WHITESPACE, *buf))
			*buf++ = 0;
		if (*buf == 0)
			break;

		// save and scan past next arg
		if (argc == MAXARGS-1) {
			cprintf("Too many arguments (max %d)\n", MAXARGS);
			return 0;
		}
		argv[argc++] = buf;
		while (*buf && !strchr(WHITESPACE, *buf))
			buf++;
	}
	argv[argc] = 0;

	// Lookup and invoke the command
	if (argc == 0)
		return 0;
	for (i = 0; i < NCOMMANDS; i++) {
		if (strcmp(argv[0], commands[i].name) == 0)
			return commands[i].func(argc, argv, tf);
	}
	cprintf("Unknown command '%s'\n", argv[0]);
	return 0;
}

void
monitor(struct Trapframe *tf)
{
	char *buf;

	cprintf("Welcome to the JOS kernel monitor!\n");
	cprintf("Type 'help' for a list of commands.\n");

	if (tf != NULL)
		print_trapframe(tf);

	while (1) {
		buf = readline("K> ");
		if (buf != NULL)
			if (runcmd(buf, tf) < 0)
				break;
	}
}
