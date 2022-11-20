#include <inc/assert.h>
#include <inc/x86.h>
#include <kern/spinlock.h>
#include <kern/env.h>
#include <kern/pmap.h>
#include <kern/monitor.h>

void sched_halt(void);

// Choose a user environment to run and run it.
void
sched_yield(void)
{
	// Implement simple round-robin scheduling.
	//
	// Search through 'envs' for an ENV_RUNNABLE environment in
	// circular fashion starting just after the env this CPU was
	// last running.  Switch to the first such environment found.
	//
	// If no envs are runnable, but the environment previously
	// running on this CPU is still ENV_RUNNING, it's okay to
	// choose that environment.
	//
	// Never choose an environment that's currently running on
	// another CPU (env_status == ENV_RUNNING). If there are
	// no runnable environments, simply drop through to the code
	// below to halt the cpu.

	// LAB 4: Your code here.

	// for fixed-priority scheduling
	uint32_t max_prio = 0;
	size_t run_pos = 0;
	bool start_flag = false;

	if(curenv){
		size_t cur_env_idx = curenv - envs;
		size_t i = (cur_env_idx + 1) % NENV;
		max_prio = curenv->env_priority;
		run_pos = cur_env_idx;

		for(;i!=cur_env_idx; i = (i+1) % NENV){
			if(envs[i].env_status == ENV_RUNNABLE && envs[i].env_priority >= max_prio){
				max_prio = envs[i].env_priority;
				run_pos = i;
				start_flag = true;
			}
		}
	} else {
		for(size_t i = 0;i<NENV;++i){
			if(envs[i].env_status == ENV_RUNNABLE && envs[i].env_priority >= max_prio){
				max_prio = envs[i].env_priority;
				run_pos = i;
				start_flag = true;
			}
		}
	}

	// Find a maximum-priority env
	if(start_flag){
		env_run(&envs[run_pos]);
	}

	// continue running the current env if no other envs are prior to it
	if(!start_flag && curenv && curenv->env_status == ENV_RUNNING){
		env_run(curenv);
	}

	// sched_halt never returns
	
	sched_halt();
}

// Halt this CPU when there is nothing to do. Wait until the
// timer interrupt wakes it up. This function never returns.
//
void
sched_halt(void)
{
	int i;

	// For debugging and testing purposes, if there are no runnable
	// environments in the system, then drop into the kernel monitor.
	for (i = 0; i < NENV; i++) {
		if ((envs[i].env_status == ENV_RUNNABLE ||
		     envs[i].env_status == ENV_RUNNING ||
		     envs[i].env_status == ENV_DYING))
			break;
	}
	if (i == NENV) {
		cprintf("No runnable environments in the system!\n");
		while (1)
			monitor(NULL);
	}

	// Mark that no environment is running on this CPU
	curenv = NULL;
	lcr3(PADDR(kern_pgdir));

	// Mark that this CPU is in the HALT state, so that when
	// timer interupts come in, we know we should re-acquire the
	// big kernel lock
	xchg(&thiscpu->cpu_status, CPU_HALTED);

	// Release the big kernel lock as if we were "leaving" the kernel
	unlock_kernel();

	// Reset stack pointer, enable interrupts and then halt.
	asm volatile (
		"movl $0, %%ebp\n"
		"movl %0, %%esp\n"
		"pushl $0\n"
		"pushl $0\n"
		// Uncomment the following line after completing exercise 13
		"sti\n"
		"1:\n"
		"hlt\n"
		"jmp 1b\n"
	: : "a" (thiscpu->cpu_ts.ts_esp0));
}

