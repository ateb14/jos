/* See COPYRIGHT for copyright information. */

#include <inc/mmu.h>
#include <inc/memlayout.h>
#include <inc/trap.h>

#include <kern/picirq.h>


###################################################################
# exceptions/interrupts
###################################################################

/* TRAPHANDLER defines a globally-visible function for handling a trap.
 * It pushes a trap number onto the stack, then jumps to _alltraps.
 * Use TRAPHANDLER for traps where the CPU automatically pushes an error code.
 *
 * You shouldn't call a TRAPHANDLER function from C, but you may
 * need to _declare_ one in C (for instance, to get a function pointer
 * during IDT setup).  You can declare the function with
 *   void NAME();
 * where NAME is the argument passed to TRAPHANDLER.
 */
#define TRAPHANDLER(name, num)						\
	.globl name;		/* define global symbol for 'name' */	\
	.type name, @function;	/* symbol type is function */		\
	.align 2;		/* align function definition */		\
	name:			/* function starts here */		\
	pushl $(num);							\
	jmp _alltraps

/* Use TRAPHANDLER_NOEC for traps where the CPU doesn't push an error code.
 * It pushes a 0 in place of the error code, so the trap frame has the same
 * format in either case.
 */
#define TRAPHANDLER_NOEC(name, num)					\
	.globl name;							\
	.type name, @function;						\
	.align 2;							\
	name:								\
	pushl $0;							\
	pushl $(num);							\
	jmp _alltraps

#define GEN_TRAPHANDLER_TEXT(num) \
	TRAPHANDLER(handler##num, num);

#define GEN_TRAPHANDLER_NOEC_TEXT(num) \
	TRAPHANDLER_NOEC(handler##num, num);

#define GEN_TRAPHANDLER_DATA(num) \
	.long handler##num;

#define GEN_TRAPHANDLER_NOEC_DATA(num) \
	.long handler##num;

#define TRAP_GENERATOR(type) \
	GEN_TRAPHANDLER_NOEC_##type(0); \
	GEN_TRAPHANDLER_NOEC_##type(1); \
	GEN_TRAPHANDLER_NOEC_##type(2); \
	GEN_TRAPHANDLER_NOEC_##type(3); \
	GEN_TRAPHANDLER_NOEC_##type(4); \
	GEN_TRAPHANDLER_NOEC_##type(5); \
	GEN_TRAPHANDLER_NOEC_##type(6); \
	GEN_TRAPHANDLER_NOEC_##type(7); \
	GEN_TRAPHANDLER_##type(8); \
	GEN_TRAPHANDLER_NOEC_##type(9); \
	GEN_TRAPHANDLER_##type(10); \
	GEN_TRAPHANDLER_##type(11); \
	GEN_TRAPHANDLER_##type(12); \
	GEN_TRAPHANDLER_##type(13); \
	GEN_TRAPHANDLER_##type(14); \
	GEN_TRAPHANDLER_NOEC_##type(15); \
	GEN_TRAPHANDLER_NOEC_##type(16); \
	GEN_TRAPHANDLER_##type(17); \
	GEN_TRAPHANDLER_NOEC_##type(18); \
	GEN_TRAPHANDLER_NOEC_##type(19); \

.text

/*
 * Lab 3: Your code here for generating entry points for the different traps.
 */

TRAP_GENERATOR(TEXT)
GEN_TRAPHANDLER_NOEC_TEXT(48) # T_SYSCALL
GEN_TRAPHANDLER_NOEC_TEXT(32) # IRQ_TIMER
GEN_TRAPHANDLER_NOEC_TEXT(33) # IRQ_KBD
GEN_TRAPHANDLER_NOEC_TEXT(36) # IRQ_SERIAL
GEN_TRAPHANDLER_NOEC_TEXT(39) # IRQ_SPURIOUS
GEN_TRAPHANDLER_NOEC_TEXT(46) # IRQ_IDE
GEN_TRAPHANDLER_NOEC_TEXT(51) # IRQ_ERROR


/*
 * Lab 3: Your code here for _alltraps
 */

_alltraps:
	pushl %ds; \
	pushl %es; \
	pushal; \
	movl $GD_KD, %eax; \
	movl %eax, %ds; \
	movl %eax, %es; \
	pushl %esp; \
	call trap
spin:
	jmp spin

/*
 * handler table
 */

.data
	.p2align	2	# force 4 byte alignment
	.globl		handlers	# make it a global variable				
handlers:
	TRAP_GENERATOR(DATA)
	GEN_TRAPHANDLER_NOEC_DATA(48) # T_SYSCALL // handlers[20], for convenience
	GEN_TRAPHANDLER_NOEC_DATA(32) # IRQ_TIMER // 21
	GEN_TRAPHANDLER_NOEC_DATA(33) # IRQ_KBD
	GEN_TRAPHANDLER_NOEC_DATA(36) # IRQ_SERIAL
	GEN_TRAPHANDLER_NOEC_DATA(39) # IRQ_SPURIOUS
	GEN_TRAPHANDLER_NOEC_DATA(46) # IRQ_IDE
	GEN_TRAPHANDLER_NOEC_DATA(51) # IRQ_ERROR
