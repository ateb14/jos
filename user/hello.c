// hello, world
#include <inc/lib.h>

void
umain(int argc, char **argv)
{
	cprintf("hello, world\n");
	cprintf("i am environment %08x\n", thisenv->env_id);

	int i = 0;
	envid_t envid[10];
	for(i = 0; i < 10; ++i){
		envid[i] = fork();
		if(envid[i] == 0){
			cprintf("Hello I am child %d with priority %u\n", i, thisenv->env_priority);
			return;
		} 
		int r = sys_set_priority(envid[i], i);
		if(r < 0){
			panic("PRIOR: %e", r);
		}
	}
}
