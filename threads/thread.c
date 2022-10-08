#include <assert.h>
#include <stdlib.h>
#include <ucontext.h>
#include "thread.h"
#include "interrupt.h"

/* Thread states */
enum { 
	RUNNING = 1,
	READY = 2,
	EXITED = 3,
	KILLED = 4,
	WAIT = 5
};

/* Gobal data structure */
Tid thread_id_list[THREAD_MAX_THREADS] = {0};
struct thread *thread_list[THREAD_MAX_THREADS] = {NULL};
struct thread *running_thread = NULL;
struct thread *ready_queue_head = NULL;
struct thread *exit_queue_head = NULL;

/* This is the wait queue structure */
struct wait_queue {
	struct thread *wait_head;
};

/* This is the thread control block */
struct thread {
	Tid thread_id;
	int thread_states;
	ucontext_t thread_context;
	void *thread_stack;
	struct thread *next;
	//added for lab 3
	struct wait_queue* thread_wait_queue;
} thread;

/* Initialize the kernel thread */
void thread_init(void)
{
	int enabled = interrupts_set(0);
	struct thread *kernel_thread = (struct thread*) malloc (sizeof(struct thread));
	assert(kernel_thread != NULL);
	getcontext(&(kernel_thread -> thread_context));
	kernel_thread -> thread_id = 0;
	kernel_thread -> thread_states = RUNNING;
	kernel_thread -> thread_stack = NULL;
	kernel_thread -> next = NULL;
	kernel_thread -> thread_wait_queue = wait_queue_create();
	thread_id_list[0] = 1;
	thread_list[0] = kernel_thread;
	running_thread = kernel_thread;
	interrupts_set(enabled);
}

/* This function returns the thread identifier of the currently running thread. 
   The return value should lie between 0 and THREAD_MAX_THREADS. See solution requirements below. */
Tid thread_id()
{
	return running_thread -> thread_id;
}

/* Clean all exit thread */
void exit_clean(){
	while (exit_queue_head != NULL){
		struct thread *current = exit_queue_head;
		exit_queue_head = exit_queue_head -> next;
		free(current -> thread_stack);
		current -> thread_stack = NULL;
		free(current);
		current = NULL;
	}
}

/* thread starts by calling thread_stub. The arguments to thread_stub are the
 * thread_main() function, and one argument to the thread_main() function. */
void
thread_stub(void (*thread_main)(void *), void *arg)
{
	interrupts_set(1);
	thread_main(arg); // call thread_main() function with arg
	thread_exit();
}

/* Helper function for create assign thread id*/
Tid allocate_thread_id(){
	int enabled = interrupts_set(0);
	for (int i = 0; i < THREAD_MAX_THREADS; i++){
		if (thread_id_list[i] == 0){
			interrupts_set(enabled);
			return i;
		}
	}
	interrupts_set(enabled);
	return THREAD_NOMORE;
}

/* Helper function for pushing into the ready queue */
void push_Ready(struct thread *new_thread){
	if (ready_queue_head == NULL){
		ready_queue_head = new_thread;
		return;
	}else{
		struct thread *current = ready_queue_head;
		while (current != NULL){
			if (current -> next == NULL){
				current -> next = new_thread;
				return;
			}
			current = current -> next;
		}
	}
};

/* Helper function for pushing into the exit queue */
void push_Exit(struct thread *new_thread){
	if (exit_queue_head == NULL){
		exit_queue_head = new_thread;
		return;
	}else{
		struct thread *current = exit_queue_head;
		while (current != NULL){
			if (current -> next == NULL){
				current -> next = new_thread;
				return;
			}
			current = current -> next;
		}
	}
};

/* Create new thread. Two invalid cases: THREAD_NOMORE & THREAD_NOMEMORY */
Tid thread_create(void (*fn) (void *), void *parg)
{
	int enabled = interrupts_set(0);
	Tid new_id = allocate_thread_id();
	if (new_id == THREAD_NOMORE){
		return THREAD_NOMORE;
	}
	struct thread *new_thread = (struct thread*) malloc (sizeof(struct thread));
	if (new_thread == NULL){
		free(new_thread);
		return THREAD_NOMEMORY;
	}
	void *stack_p = (void *) malloc (THREAD_MIN_STACK);
	if (stack_p == NULL){
		free(new_thread);
		free(stack_p);
		return THREAD_NOMEMORY;
	}
	getcontext(&(new_thread -> thread_context));
	new_thread -> thread_id = new_id;
	new_thread -> thread_states = READY;
	new_thread -> thread_stack = stack_p;
	new_thread -> next = NULL;
	new_thread -> thread_wait_queue = wait_queue_create();
	thread_id_list[new_id] = 1;
	thread_list[new_id] = new_thread;
	new_thread -> thread_context.uc_mcontext.gregs[REG_RSP] = (unsigned long) (stack_p + THREAD_MIN_STACK - 8);
	new_thread -> thread_context.uc_mcontext.gregs[REG_RBP] = (unsigned long) stack_p;
	new_thread -> thread_context.uc_mcontext.gregs[REG_RDI] = (unsigned long) fn;
	new_thread -> thread_context.uc_mcontext.gregs[REG_RSI] = (unsigned long) parg;
	new_thread -> thread_context.uc_mcontext.gregs[REG_RIP] = (unsigned long) &thread_stub;
	push_Ready(new_thread);
	interrupts_set(enabled);
	return new_id;
}

/* Helper function for popping from the ready queue head */
struct thread *pop_Ready_head(){
	struct thread *pop_head = ready_queue_head;
	ready_queue_head = pop_head -> next;
	pop_head -> next = NULL;
	return pop_head;
}

/* Helper function for popping a specific thread from the ready queue */
struct thread *pop_from_Ready(Tid pop_thread_id){
	if (thread_list[pop_thread_id] == ready_queue_head){
		return pop_Ready_head();
	}
	struct thread *current = ready_queue_head;
	while (current -> next -> thread_id != pop_thread_id){
		current = current -> next;
	}
	struct thread *temp = current -> next;
	current -> next = temp -> next;
	temp -> next = NULL;
	return temp;
}

/* yield */
Tid thread_yield(Tid want_tid)
{
	int enabled = interrupts_set(0);
	if (want_tid == THREAD_SELF || running_thread -> thread_id == want_tid){
		running_thread -> thread_states = READY;
		int setcontext_called = 0;
		getcontext(&(running_thread -> thread_context));
		if (setcontext_called == 0){
			setcontext_called = 1;
			running_thread -> thread_states = RUNNING;
			setcontext(&(running_thread -> thread_context));
		}
		interrupts_set(enabled);
		return running_thread -> thread_id;
	} else if (want_tid == THREAD_INVALID || want_tid >= THREAD_MAX_THREADS || want_tid < THREAD_FAILED){
	 	//either out of bounds or invalid or the thread is not created yet
	 	interrupts_set(enabled);
		return THREAD_INVALID;
    } else if (want_tid == THREAD_ANY){
		if (ready_queue_head == NULL){
			interrupts_set(enabled);
			return THREAD_NONE;
		}
		exit_clean();
		struct thread *ready_to_run = pop_Ready_head();
		struct thread *current_running = running_thread;
		int setcontext_called = 0;	
		running_thread = ready_to_run;
		ready_to_run -> thread_states = RUNNING;
		if (current_running -> thread_states == RUNNING){
			current_running -> thread_states = READY;
			push_Ready(current_running);
			getcontext(&(current_running -> thread_context));
			if (setcontext_called == 0){
				setcontext_called = 1;
				setcontext(&(ready_to_run -> thread_context));
			}
		}else{
			thread_id_list[current_running -> thread_id] = 0;
			thread_list[current_running -> thread_id] = NULL;
			push_Exit(current_running);
			if (setcontext_called == 0){
				setcontext_called = 1;
				setcontext(&(ready_to_run -> thread_context));
			}
		}
		interrupts_set(enabled);
		return ready_to_run -> thread_id;
	} else {
		if (thread_id_list[want_tid] == 0){
			interrupts_set(enabled);
			return THREAD_INVALID;
		}
		if (ready_queue_head == NULL){
			interrupts_set(enabled);
			return THREAD_NONE;
		}	
		struct thread *ready_to_run = pop_from_Ready(want_tid);
		struct thread *current_running = running_thread;
		current_running -> thread_states = READY;
		push_Ready(current_running);
		running_thread = ready_to_run;
		ready_to_run -> thread_states = RUNNING;
		int setcontext_called = 0;	
		getcontext(&(current_running -> thread_context));
		if (setcontext_called == 0){
			setcontext_called = 1;
			setcontext(&(ready_to_run -> thread_context));
		}
		interrupts_set(enabled);
		return ready_to_run -> thread_id;
	}
}

/* current thread does not run after this call */
void thread_exit()
{
	int enabled = interrupts_set(0);
	if (running_thread -> thread_wait_queue != NULL){
		thread_wakeup(running_thread -> thread_wait_queue, 1);
		wait_queue_destroy(running_thread -> thread_wait_queue);
	}
	if (ready_queue_head != NULL){
		running_thread -> thread_states = EXITED;
		thread_yield(THREAD_ANY);
		interrupts_set(enabled);
		return;
	}else{
		struct thread *current_running = running_thread;
		current_running -> thread_states = EXITED;
		running_thread = NULL;
		thread_id_list[current_running -> thread_id] = 0;
		thread_list[current_running -> thread_id] = NULL;
		free(current_running->thread_stack);
		current_running->thread_stack = NULL;
		free(current_running);
		current_running = NULL;
		exit(0);
		interrupts_set(enabled);
		return;
	}
}

/* This function kills another thread whose identifier is tid.  */
Tid thread_kill(Tid tid)
{
	int enabled = interrupts_set(0);
	if (tid == THREAD_INVALID || tid >= THREAD_MAX_THREADS || tid < THREAD_FAILED  || thread_id_list[tid] == 0 || thread_list[tid] -> thread_states == RUNNING){
		interrupts_set(enabled);
	 	return THREAD_INVALID;
	}
	if (thread_list[tid] -> thread_states == READY || thread_list[tid] -> thread_states == WAIT){
		thread_list[tid] -> thread_states = KILLED;
		thread_list[tid] -> thread_context.uc_mcontext.gregs[REG_RIP] = (unsigned long) &thread_exit;
		interrupts_set(enabled);
		return tid;
	}else{
		interrupts_set(enabled);
		return THREAD_INVALID;
	}
	interrupts_set(enabled);
	return THREAD_FAILED;
}

/*******************************************************************
 * Important: The rest of the code should be implemented in Lab 3. *
 *******************************************************************/

/* make sure to fill the wait_queue structure defined above */
struct wait_queue *wait_queue_create()
{
	struct wait_queue *wq;

	wq = malloc(sizeof(struct wait_queue));
	assert(wq);

	wq->wait_head = NULL;

	return wq;
}

void wait_queue_destroy(struct wait_queue *wq)
{
	if (wq == NULL){
		return;
	}
	struct thread *temp_ptr = wq->wait_head;
	while (temp_ptr != NULL){
		struct thread *delete = temp_ptr;
		temp_ptr = temp_ptr -> next;
		free(delete -> thread_stack);
		delete -> thread_stack = NULL;
		free(delete);
		delete = NULL;
	}
	free(wq);
}

/* Helper function for pushing into the wait queue */
void push_Wait(struct thread *new_thread, struct wait_queue *queue){
	if (queue -> wait_head == NULL){
		queue -> wait_head = new_thread;
		return;
	}else{
		struct thread *current = queue -> wait_head;
		while (current != NULL){
			if (current -> next == NULL){
				current -> next = new_thread;
				return;
			}
			current = current -> next;
		}
	}
}

/* Helper function for popping from the wait queue head */
struct thread *pop_Wait_head(struct wait_queue *queue){
	if (queue == NULL || queue -> wait_head == NULL){
		return NULL;
	}
	struct thread *pop_head = queue -> wait_head;
	queue -> wait_head = pop_head -> next;
	pop_head -> next = NULL;
	return pop_head;
}

Tid thread_sleep(struct wait_queue *queue)
{
	int enabled = interrupts_set(0);
	if (queue == NULL){
		interrupts_set(enabled);
		return THREAD_INVALID;
	}else if (ready_queue_head == NULL){
		interrupts_set(enabled);
		return THREAD_NONE;
	}else{
		struct thread *ready_to_run = pop_Ready_head();
		struct thread *current_running = running_thread;
		volatile int setcontext_called = 0;	
		running_thread = ready_to_run;
		if(ready_to_run ->thread_states != KILLED){
			ready_to_run -> thread_states = RUNNING;
		}
		current_running -> thread_states = WAIT;
		push_Wait(current_running, queue);    
		getcontext(&(current_running -> thread_context));
		if (setcontext_called == 0){
			setcontext_called = 1;
			setcontext(&(ready_to_run -> thread_context));
		}
		interrupts_set(enabled);
		return ready_to_run -> thread_id;
	}
}

/* when the 'all' parameter is 1, wakeup all threads waiting in the queue.
 * returns whether a thread was woken up on not. */
int thread_wakeup(struct wait_queue *queue, int all)
{
	int enabled = interrupts_set(0);
	if (queue == NULL){
		interrupts_set(enabled);
		return 0;
	}else if (queue -> wait_head == NULL){
		interrupts_set(enabled);
		return 0;
	}else{
		if (all == 0){
			struct thread *wakeup_node = pop_Wait_head(queue);
			if (wakeup_node -> thread_states != KILLED){
				wakeup_node -> thread_states = READY;
			}
			push_Ready(wakeup_node);
			interrupts_set(enabled);
			return 1;
		}else if (all == 1){
			int counter = 0;
			while (queue -> wait_head != NULL){
				struct thread *wakeup_node = pop_Wait_head(queue);
				if (wakeup_node -> thread_states != KILLED){
					wakeup_node -> thread_states = READY;
				}
				push_Ready(wakeup_node);
				counter++;
			}
			interrupts_set(enabled);
			return counter;
		}
	}
	interrupts_set(enabled);
	return 0;
}

Tid thread_wait(Tid tid)
{
	int enabled = interrupts_set(0);  
  	if(tid < 0 || tid >= THREAD_MAX_THREADS || tid == running_thread -> thread_id){
  		interrupts_set(enabled);
		return THREAD_INVALID;
  	}
	struct thread * want = thread_list[tid];
    if(want == NULL|| want->thread_states == EXITED){
        interrupts_set(enabled);
	return THREAD_INVALID;
    }
    thread_sleep(want->thread_wait_queue);
	interrupts_set(enabled);
	return tid;
}

struct lock {
	struct wait_queue* wait_q;
	Tid lock_tid;
};

struct lock * lock_create()
{
	int enabled = interrupts_set(0);
	struct lock *lock;
	lock = malloc(sizeof(struct lock));
	assert(lock);
	lock -> wait_q = wait_queue_create();
	lock -> lock_tid = THREAD_NONE;
	interrupts_set(enabled);
	return lock;
}

void lock_destroy(struct lock *lock)
{
	int enabled = interrupts_set(0);
	assert(lock != NULL);
	wait_queue_destroy(lock -> wait_q);
	interrupts_set(enabled);
	free(lock);
}

void lock_acquire(struct lock *lock)
{
	int enabled = interrupts_set(0);
	assert(lock != NULL);
	while (lock -> lock_tid != THREAD_NONE){
		thread_sleep(lock -> wait_q);
	}
	lock -> lock_tid = running_thread -> thread_id;
	interrupts_set(enabled);
}

void lock_release(struct lock *lock)
{
	int enabled = interrupts_set(0);
	assert(lock != NULL);
	thread_wakeup(lock -> wait_q, 1);
	lock -> lock_tid = THREAD_NONE;
	interrupts_set(enabled);
}

struct cv {
	struct wait_queue *wait_q;
};

struct cv *cv_create()
{
	int enabled = interrupts_set(0);
	struct cv *cv;
	cv = malloc(sizeof(struct cv));
	assert(cv);
	cv -> wait_q = wait_queue_create();
	interrupts_set(enabled);
	return cv;
}

void cv_destroy(struct cv *cv)
{
	int enabled = interrupts_set(0);
	assert(cv != NULL);
	wait_queue_destroy(cv -> wait_q);
	free(cv);
	interrupts_set(enabled);
}

void cv_wait(struct cv *cv, struct lock *lock)
{
	int enabled = interrupts_set(0);
	assert(cv != NULL);
	assert(lock != NULL);
	lock_release(lock);
	thread_sleep(cv -> wait_q);
	lock_acquire(lock);
	interrupts_set(enabled);
}

void cv_signal(struct cv *cv, struct lock *lock)
{
	int enabled = interrupts_set(0);
	assert(cv != NULL);
	assert(lock != NULL);
	if (cv -> wait_q -> wait_head != NULL){
		thread_wakeup(cv -> wait_q, 0);
	}
	interrupts_set(enabled);
}

void cv_broadcast(struct cv *cv, struct lock *lock)
{
	int enabled = interrupts_set(0);
	assert(cv != NULL);
	assert(lock != NULL);
	if (cv -> wait_q -> wait_head != NULL){
		thread_wakeup(cv -> wait_q, 1);
	}
	interrupts_set(enabled);
}
