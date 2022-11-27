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
	{ "backtrace", "Display the backtrace information in the stack", mon_backtrace},
	{ "colors", "Display all the colors we have", mon_colors},
	{ "sm", "Display the physical page mappings by pages", mon_showmappings},
	{"spp", "Set the permissions of the selected mapping", mon_setpagepermission},
	{"dump", "Dump the contents of a range of memory given either a virtual or physical address range", mon_dumppage},
	{"si", "debug: step in", mon_stepin},
	{"c", "debug: continue", mon_continue},
};

/***** Implementations of basic kernel monitor commands *****/

int mon_stepin(int argc, char **argv, struct Trapframe *tf){
	if(!tf){
		cprintf("Debug mode can be turned on only when there is a trap!\n");
		return 0;
	}
	if((tf->tf_cs & 3) != 3){
		cprintf("Debug mode can only be turned on due to a trap caused by a user environment!\n");
		return 0;
	}
	if(tf->tf_trapno != T_BRKPT && tf->tf_trapno != T_DEBUG){
		cprintf("The trap frame doesn't include a debug sign!\n");
		return 0;
	}
	tf->tf_eflags |= FL_TF;
	return -1; // kill the monitor
}

int mon_continue(int argc, char **argv, struct Trapframe *tf){
	if(!tf){
		cprintf("Debug mode can be turned on only when there is a trap!\n");
		return 0;
	}
	if((tf->tf_cs & 3) != 3){
		cprintf("Debug mode can only be turned on due to a trap caused by a user environment!\n");
		return 0;
	}
	if(tf->tf_trapno != T_BRKPT && tf->tf_trapno != T_DEBUG){
		cprintf("The trap frame doesn't include a debug sign!\n");
		return 0;
	}
	tf->tf_eflags &= ~FL_TF;
	return -1;
}

int mon_dumppage(int argc, char **argv, struct Trapframe *tf){
	if(argc <= 2){
		goto usage;
	}
	if(strlen(argv[1]) <= 1){
		goto usage;
	}
	char mode = argv[1][1];
	size_t times = 0;
	uint32_t decode_range = 1;
	// parse the range
	int pos = 3;
	for(;pos<argc;++pos){
		if(strlen(argv[pos]) <= 1){
			goto usage;
		}
		switch(argv[pos][0]){
		case '+':
		times = (size_t)strtol(argv[pos]+1, NULL, 0);break;
		case 'd':
		decode_range = (uint32_t)strtol(argv[pos]+1, NULL, 0);
		if(!(decode_range == 1 || decode_range == 2 || decode_range == 4 || decode_range == 8)){
			goto usage;
		}
		break;
		}
	}

	// parse the mode
	if(mode == 'p'){ // PA
		size_t i;
		physaddr_t pa = (physaddr_t)strtol(argv[2], NULL, 0);
		for(i=0;i<=times;++i,++pa){
			if(pa < npages * PGSIZE){
				switch(decode_range){
					case 1:
					cprintf("PA:0x%08x\tVal:0x%x\n", pa, *(uint8_t *)KADDR(pa));break;
					case 2:
					cprintf("PA:0x%08x\tVal:0x%x\n", pa, *(uint16_t *)KADDR(pa));break;
					case 4:
					cprintf("PA:0x%08x\tVal:0x%x\n", pa, *(uint32_t *)KADDR(pa));break;
					case 8:
					cprintf("PA:0x%08x\tVal:0x%x\n", pa, *(uint64_t *)KADDR(pa));break;
				}
			} else {
				cprintf("PA:0x%08x\tVal:PA out of range\n", pa);
			}
		}
	} else if(mode == 'v'){ // VA
		uintptr_t vm_addr = (uintptr_t)strtol(argv[2], NULL, 0);
		size_t i;
		bool no_mapping_only_shows_once = false;
		for(i=0;i<=times;++i, ++vm_addr){
			pte_t *pte = pgdir_walk(kern_pgdir, (void *)vm_addr, false);
			if((!pte || !(*pte & PTE_P))){
				if(!no_mapping_only_shows_once){
					cprintf("VA:0x%08x\tPA:----------\tVal:--\n", vm_addr);
					no_mapping_only_shows_once = true;
				}
				continue;
			}
			physaddr_t pa = *pte & PTE_PS ? PDE_ADDR_BIG_PAGE(*pte) + PGOFF_BIG_PAGE(vm_addr) : PTE_ADDR(*pte) + PGOFF(vm_addr);
			// check whether the PA is out of range
			if(pa < npages * PGSIZE){
				no_mapping_only_shows_once = false;
				switch(decode_range){
				case 1:
				cprintf("VA:0x%08x\tPA:0x%08x\tVal:0x%x\n", vm_addr, pa, *(uint8_t *)KADDR(pa));break;
				case 2:
				cprintf("VA:0x%08x\tPA:0x%08x\tVal:0x%x\n", vm_addr, pa, *(uint16_t *)KADDR(pa));break;
				case 4:
				cprintf("VA:0x%08x\tPA:0x%08x\tVal:0x%x\n", vm_addr, pa, *(uint32_t *)KADDR(pa));break;
				case 8:
				cprintf("VA:0x%08x\tPA:0x%08x\tVal:0x%x\n", vm_addr, pa, *(uint64_t *)KADDR(pa));break;
				}
			} else {
				cprintf("VA:0x%08x\tPA:0x%08x\tVal:PA out of range\n", vm_addr, pa);
			}
		}
	} else {
		goto usage;
	}
	
	return 0;
usage:
	cprintf("usage: dump <-p/-v> <addr> <optional: +<$byte_offset>> <optinal: d<$decode_range(1,2,4 or 8)>>\n");
	return 0;
}

int 
mon_setpagepermission(int argc, char **argv, struct Trapframe *tf){
	if(argc <= 2){
		cprintf("usage: spp <vm_addr> <mode> <permission1> <permission2>...\n");
		cprintf("modes: clear, cover, add, delete\n");
		cprintf("permissions: W, U\n");
		return 0;
	}
	uintptr_t vm_addr = (uintptr_t)strtol(argv[1], NULL, 0);
	pte_t *pte = pgdir_walk(kern_pgdir, (void *)vm_addr, false);
	if(!pte || !(*pte & PTE_P)){
		cprintf("The mapping doesn't exist!\n");
		return 0;
	}
	if(strcmp(argv[2],"clear") == 0){
		*pte &= ~(PTE_W | PTE_U );
		return 0;
	}
	uint32_t mask=0;
	size_t i;
	for(i=3;i<argc;++i){
		switch(argv[i][0]){
			case 'W': mask |= PTE_W; break;
			case 'U': mask |= PTE_U; break;
			case 'A': mask |= PTE_A; break;
		}
	}
	if(strcmp(argv[2],"cover") == 0){
		*pte &= ~(PTE_W | PTE_U );
		*pte |= mask;
	} else if(strcmp(argv[2],"add") == 0){
		*pte |= mask;
	} else if(strcmp(argv[2],"delete") == 0){
		*pte &= ~mask;
	} else {
		cprintf("Illegal input!\n");
		cprintf("modes: clear, cover, add, delete\n");
	}
	return 0;
}

int 
mon_showmappings(int argc, char **argv, struct Trapframe *tf){
	if(argc == 1){
		cprintf("usage: sm <vm_left> <optional: vm_right>/<optional: +page_offset> <optional: r>(round down to page size)\n");
		return 0;
	}
	uintptr_t vm_left = (uintptr_t)strtol(argv[1], NULL, 0);
	uintptr_t vm_right = vm_left;
	char ctrl = 0;
	if(argc >= 3){
		if(argv[2][0] >= '0' && argv[2][0] <= '9'){
			vm_right = (uintptr_t)strtol(argv[2], NULL, 0);
		} else{
			ctrl = argv[2][0];
		}
		if(ctrl == '+'){
			vm_right = vm_left + (uintptr_t)strtol(argv[2]+1, NULL, 0) * PGSIZE;
			if(vm_right < vm_left){ // overflow
				vm_right = 0xfffff000 + PGOFF(vm_left);
			}
		}
	}
	if(argc >= 4){
		ctrl = argv[3][0];
	}
	switch(ctrl){
		case 'r': 
		vm_left = ROUNDDOWN(vm_left, PGSIZE);
		vm_right = ROUNDDOWN(vm_right, PGSIZE);
		break;
	}
	if(vm_right < vm_left){
		vm_right = vm_left;
	}
	uintptr_t vm;
	bool only_show_once_flag = false;
	size_t times = (vm_right - vm_left) / PGSIZE + 1;
	size_t i = 0;
	for(vm = vm_left; i < times; vm+=PGSIZE, ++i){
		pte_t *pte = pgdir_walk(kern_pgdir, (void *)vm, false);
		if(pte && (*pte & PTE_P)){
			physaddr_t pa = *pte & PTE_PS ? PDE_ADDR_BIG_PAGE(*pte) + PGOFF_BIG_PAGE(vm) : PTE_ADDR(*pte) + PGOFF(vm);
			cprintf("VA:0x%08x\tPA:0x%08x\tW:%d U:%d\n", vm, pa, !!(*pte & PTE_W), !!(*pte & PTE_U));
			only_show_once_flag = false;
		} else if(!only_show_once_flag){
			cprintf("VA:0x%08x\tPA:----------\tW:- U:-\n", vm);
			only_show_once_flag = true;
		}
	}
	return 0;
}

int
mon_colors(int argc, char **argv, struct Trapframe *tf){
	for(int i=0;i<=0xf;++i){
		for(int j=0;j<=0xf;++j){
			set_color(i,j);
			cprintf(" %2d ",i+j);
		}
		back_to_default_color();
		cprintf("\n");
	}
	back_to_default_color();
	cprintf("\n");
	return 0;
}

int
mon_help(int argc, char **argv, struct Trapframe *tf)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(commands); i++)
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
	// Your code here.
	cprintf("Stack backtrace:\n");
	uint32_t ebp, eip;
	uint32_t args[5]={0};
	asm volatile("movl %%ebp, %0" : "=r" (ebp));
	while(ebp != 0){
		uint32_t *pebp = (uint32_t *)ebp;
		eip = pebp[1];
		for(int i=0;i<5;++i){
			if ((uintptr_t)(pebp + i + 2) >= USTACKTOP){
				break;
			}
			args[i] = pebp[2+i];
		}
		cprintf("ebp %08x  eip %08x  args %08x %08x %08x %08x %08x\n",ebp, eip, args[0], args[1], args[2], args[3], args[4]);
		struct Eipdebuginfo info;
		debuginfo_eip((uintptr_t) eip, &info);
		cprintf("	%s:%d: %.*s+%d\n", info.eip_file, info.eip_line, info.eip_fn_namelen, info.eip_fn_name, eip-info.eip_fn_addr);
		ebp = pebp[0];
	}
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
	for (i = 0; i < ARRAY_SIZE(commands); i++) {
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
