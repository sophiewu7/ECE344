#include <sys/types.h>
#include <unistd.h>
#include <assert.h>
#include <stdlib.h>
#include <ucontext.h>
#include "interrupt.h"

#ifndef __x86_64__
#error "Do this project on a 64-bit x86-64 linux machine"
#endif /* __x86_64__ */

#if __WORDSIZE != 64
#error "word size should be 64 bits"
#endif

static void call_setcontext(ucontext_t * context);
static void show_interrupt(void);

/* zero out the context */
ucontext_t mycontext = { 0 };

void
get_start_end(long *startp, long *endp)
{
	char filename[128];
	char line[256];
	FILE *f;
	
	sprintf(filename, "/proc/%d/maps", getpid());
	f = fopen(filename, "r");
	assert(f);
	/* read first line */
	fgets(line, sizeof(line), f);
	/* read second line, which has the executable region. */
	fgets(line, sizeof(line), f);	
	sscanf(line, "%lx-%lx ", startp, endp);
}

int
main(int argc, char **argv)
{
	long start, end;
	/* we declare this variable to be volatile so that the compiler will
	 * make sure to store it on the stack and not in a register. This is
	 * ESSENTIAL, or else the code in this function may run in an infinite
	 * loop (you can try this by removing the volatile keyword, and
	 * compiling this file with the "-O2" flag to the gcc compiler, by
	 * changing CFLAGS in the Makefile).
	 *
	 * QUESTION: why must setcontext_called be stored on the stack, and not
	 * in a register? You will need to look at the code below, and
	 * understand how getcontext and setcontext work to answer this
	 * question. */
	volatile int setcontext_called = 0;
	int err;

	/*
	 * DO NOT CHANGE/ADD ANY CODE UNTIL BELOW TBD(). SEE BELOW.
	 */

	/* Get context: make sure to read the man page of getcontext in detail,
	 * or else you will not be able to answer the questions below. */
	err = getcontext(&mycontext);
	assert(!err);

	// When successful, getcontext() return 0 and setcontext does not return
	// When error, both return -1 and set errno appropriately
	// So the above is to check if we correctly get context from my context
	// getcontext() initializes the structure pointed by the ucp(mycontext) to the currently active context
	// we declared mycontext above as ucontext_t mycontext = { 0 };

	/* QUESTION: which of the fields of mycontext changed due to the call
	 * above? Hint: It will help to run the program using gdb and put a
	 * breakpoint at entry to main and before and after the calls to
	 * getcontext().
	 * - Use "info registers" to see the values of the registers.
	 * - Use "next"/"step" to advance to the next line of code.
	 * - Use "print mycontext" to see the values stored in mycontext.
	 *   Compare them with the output of "info registers".
	 * - Use "ptype mycontext" so see the type/fields of mycontext */

	// uc_mcontext changed
	// __FUNCTION__ is another name for __func__ , provided for backward compatibility with old versions of GCC. 
	// These identifiers are variables, not preprocessor macros, and may not be used to initialize char arrays or 
	// be concatenated with string literals.

	printf("%s: setcontext_called = %d\n", __FUNCTION__, setcontext_called);
	if (setcontext_called == 1) {
		/* QUESTION: will be get here? why or why not? */
		// Yes!!! When setcontext_called at line 155 The code will jump here
		show_interrupt();
		exit(0);
	}

	get_start_end(&start, &end);
	printf("start = 0x%lx\n", start);
	printf("end = 0x%lx\n", end);
	/*
	 * comment out TBD, and replace the -1 in each printf() with what is
	 * expected to be printed.
	 */
	//TBD();

	/* show size of ucontext_t structure. Hint: use sizeof(). */
	printf("ucontext_t size = %ld bytes\n", (long int) sizeof (ucontext_t));

	/* now, look inside of the context you just saved. */

	/* first, think about code */
	/* the program counter is called rip in x86-64 */
	printf("memory address of main() = 0x%lx\n", (unsigned long) &main);
	printf("memory address of the program counter (RIP) saved "
	       "in mycontext = 0x%lx\n",
	       (unsigned long) mycontext.uc_mcontext.gregs[REG_RIP]);

	/* now, think about parameters */
	printf("argc = %d\n", argc);
	printf("argv = %p\n", (void *) argv);
	/* QUESTIONS: how are these parameters passed into the main function? 
	 * are there any saved registers in mycontext that store the parameter
	 * values above. why or why not? Hint: Use gdb, and then run
	 * "disassemble main" in gdb, and then scroll up to see the beginning of
	 * the main function. */ 
	// By looking at the assembly code, I think argc is stored in edi and argv is stored in rsi

	/* now, think about the stack */
	/* QUESTIONS: Is setcontext_called and err stored on the stack? does the
	 * stack grow up or down? What are the stack related data in
	 * mycontext.uc_mcontext.gregs[]? */
	// They are stored on the stack. The stack grow down
	printf("memory address of the variable setcontext_called = %p\n",
	       (void *) &setcontext_called);
	printf("memory address of the variable err = %p\n",
	       (void *) &err);
	printf("number of bytes pushed to the stack between setcontext_called "
	       "and err = %ld\n", (unsigned long) &setcontext_called - (unsigned long) &err);

	printf("stack pointer register (RSP) stored in mycontext = 0x%lx\n",
	       (unsigned long) mycontext.uc_mcontext.gregs[REG_RSP]);

	printf("number of bytes between err and the saved stack in mycontext "
	       "= %ld\n", (unsigned long) &err - (unsigned long) mycontext.uc_mcontext.gregs[REG_RSP]);

	/* QUESTION: what is the uc_stack field in mycontext? Note that this
	 * field is used to store an alternate stack for use during signal
	 * handling, and is NOT the stack of the running thread. */
	printf("value of uc_stack.ss_sp = 0x%lx\n",
	       (unsigned long) mycontext.uc_stack.ss_sp);

	/* Don't move on to the next part of the lab until you know how to
	 * change the stack in a context when you manipulate a context to create
	 * a new thread. */

	/* now we will try to understand how setcontext works */
	setcontext_called = 1;
	call_setcontext(&mycontext);
	/* QUESTION: why does the program not fail at the assert below? */
	// Because the program jumped to line 88
	assert(0);
}

static void
call_setcontext(ucontext_t * context)
{
	int err = setcontext(context);
	assert(!err);
}

static void
show_interrupt(void)
{
	int err;

	/* QUESTION: how did we get here if there was an assert above? */

	/* now think about interrupts. you will need to understand how they
	 * work, and how they interact with get/setcontext for implementing
	 * preemptive threading. */

	/* QUESTION: what does interrupts_on() do? see interrupt.c */
	interrupts_on();

	/* getcontext stores the process's signal mask */
	err = getcontext(&mycontext);
	assert(!err);

	/* QUESTION: Are interrupts masked (i.e., disabled) in mycontext?
	 * HINT: use sigismember below. */
	// SIG_TYPE is = SIGALRM in interrupt.h
	// SIGALRM is an asynchronous signal. 
	// The SIGALRM signal is raised when a time interval specified in a call to the alarm or alarmd function expires. 
	// Because SIGALRM is an asynchronous signal, the SAS/C library discovers the signal only when you call a function, 
	// when a function returns, or when you issue a call to sigchk .

	printf("interrupt is disabled = %d\n",
	       (unsigned int) sigismember(&mycontext.uc_sigmask, SIG_TYPE));

	interrupts_off();

	err = getcontext(&mycontext);
	assert(!err);

	/* QUESTION: which fields of mycontext changed as a result of the
	 * getcontext call above? */
	printf("interrupt is disabled = %d\n",
	       (unsigned int) sigismember(&mycontext.uc_sigmask, SIG_TYPE));
}
