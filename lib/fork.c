// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.
	// check the presence of the PTE
	if(!(uvpd[PDX(addr)] & PTE_P) || !(uvpt[PGNUM(addr)]) & PTE_P){
		panic("pgfault: invalid address %p, pde: %p, pte: %p", addr, uvpd[PDX(addr)], uvpt[PGNUM(addr)]);
	}
	if(!(err & FEC_WR && uvpt[PGNUM(addr)] & PTE_COW && uvpt[PGNUM(addr)] & PTE_U)){
		panic("pgfault: permission violation!");
	}

	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.

	// LAB 4: Your code here.

	// allocate a new page
	int res = sys_page_alloc(0, (void *)PFTEMP, PTE_U | PTE_P | PTE_W );
	if(res < 0){
		panic("pgfault: out of memory!");
	}
	// copy the data
	void * addr_pgalign = (void *) ROUNDDOWN(addr, PGSIZE);
	memcpy((void *) PFTEMP, addr_pgalign, PGSIZE);

	// remap
	res = sys_page_map(0, (void *) PFTEMP, 0, addr_pgalign, PTE_U | PTE_P | PTE_W);
	if(res < 0){
		panic("pgfault: remap failed!");
	}

	// unmap the temporary mapping
	res = sys_page_unmap(0, (void *) PFTEMP);
	if(res < 0){
		panic("pgfault: unmap failed!");
	}
}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn)
{
	int r;

	// LAB 4: Your code here.
	void * addr = (void *) (pn * PGSIZE);

	assert((int32_t) addr < UXSTACKTOP - PGSIZE);

	// check the presence of the PTE
	if(!(uvpd[PDX(addr)] & PTE_P) || !(uvpt[PGNUM(addr)]) & PTE_P){
		panic("duppage: invalid address %p", addr);
	}
	// check the permissions
	if(!(uvpd[PDX(addr)] & PTE_U)){
		panic("duppage: permission violation!");
	}


	int perm = 0;
	if(uvpt[PGNUM(addr)] & PTE_W || uvpt[PGNUM(addr) & PTE_COW]){
		// reset the permissions of the child
		r = sys_page_map(0, addr, envid, addr, PTE_COW | PTE_P | PTE_U);
		if(r < 0){
			return r;
		}

		// reset the permissions of the current environment
		r= sys_page_map(0, addr, 0, addr, PTE_COW | PTE_P | PTE_U);
		if(r < 0){
			return r;
		}
	} else {
		// only reset the permissions of the child
		r = sys_page_map(0, addr, envid, addr, PTE_P | PTE_U);
		if(r < 0){
			return r;
		}
	}

	return 0;
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	// LAB 4: Your code here.
	set_pgfault_handler(pgfault);
	envid_t envid = sys_exofork();
	if(envid == 0){
		// child
		thisenv = &envs[ENVX(sys_getenvid())];
	} else if(envid > 0){
		// parent
		size_t pg = 0;
		while(pg < ((UXSTACKTOP - PGSIZE) / PGSIZE)){ // Below the user exception stack
			// check the page directory
			if(!(uvpd[pg / NPTENTRIES] & PTE_P)){
				// skip the empty page table mapped from this directory entry
				pg += NPTENTRIES;
				continue;
			}
			// scan all the page table entries mapped from this directory entry
			size_t end = pg + NPTENTRIES >= ((UXSTACKTOP - PGSIZE) / PGSIZE) ? ((UXSTACKTOP - PGSIZE) / PGSIZE) : pg + NPTENTRIES;
			for(; pg < end; ++pg){
				if(uvpt[pg] & PTE_U && uvpt[pg] & PTE_P){
					int r = duppage(envid, pg);
					if(r < 0){
						panic("fork: duppage error!");
					}
				}
			}
		}
		// allocate an exception stack for the child
		if (sys_page_alloc(envid, (void *) UXSTACKTOP - PGSIZE, PTE_W | PTE_U | PTE_P) < 0){
			panic("fork: sys_page_alloc failed!");
		}
		// set the page fault upcall for the child
		extern void _pgfault_upcall(void);
		if (sys_env_set_pgfault_upcall(envid, _pgfault_upcall) < 0){
			panic("fork: sys_env_set_pgfault_upcall failed!");
		}
		// make the child runnable
		if (sys_env_set_status(envid, ENV_RUNNABLE) < 0){
			panic("fork: sys_env_set_status failed!");
		}
	} else {
		panic("fork: sys_exofork error!");
	}
	return envid;
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
