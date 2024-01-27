	.text

/* Switch from current_thread to next_thread. Make next_thread
 * the current_thread, and set next_thread to 0.
 * Use eax as a temporary register; it is caller saved.
 */
	.globl thread_switch
// The first time thread_switch is called, thread_switch switch from main() to user_thread
// The last time thread_switch is called, thread_switch switch from user_thread to main()
thread_switch:
    // pushal pushes registers to the thread's own stack
    // e.g. the main() has its own stack
    // each user_thread has its own stack
    pushal

    // *current_thread is current_thread->sp
    // current_thread->sp = %esp
    // store %esp in current_thread->sp
    movl current_thread, %eax
    movl %esp, (%eax)

    // *next_thread is next_thread->sp
    // %esp = next_thread->sp
    // resume %esp from the last time thread_switch is called
    // the first time this is called, next_thread->sp is the address set by the caller
    movl next_thread, %eax
    movl (%eax), %esp

    // current_thread = next_thread
    movl %eax, current_thread

    // next_thread = 0
    movl $0, next_thread

    // The first time popal is called, it pops some values set by the caller
    popal
    ret
