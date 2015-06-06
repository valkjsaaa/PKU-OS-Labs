#include <inc/lib.h>

void
umain(int argc, char **argv)
{
	cprintf("syscall returned:%d\n", sys_test(0));
}