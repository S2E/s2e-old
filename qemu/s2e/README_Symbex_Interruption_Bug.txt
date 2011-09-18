#####################################################################################
################################# Issue Introduction ################################
#####################################################################################

1. Consider the following bug, if you run into weird problems (e.g., unusual kernel panics).
2. The bug was discovered for an ARM target.
3. It is possible that it also applies to X86. 
4. The appearance of the bug is correlated with the following condition in ARM: 
   (m_startSymbexAtPC != -1 && m_startSymbexAtPC != state->getPc()) when S2E runs the method: 
   executeTranslationBlock in S2EExecutor.cpp.
5. Concretely, it occurs while running an Android software stack and executing the following method of an Android application.
   public void onCallNative1(View view) {
        // call to the native method of the S2EAndroid Wrapper which injects a symbolic value
        int test = S2EAndroidWrapper.getSymbolicInt("testvar");
        Log.d(DEBUG_TAG, "Back from journey to S2E. Nice trip.");
        test+=1;
        Log.d(DEBUG_TAG, "Back from test+=1.");  
    }
    This java code is converted into dex bytecode. A Dalvik interpreter interprets the bytecode and eventually calls a method of a native library. 
6. After calling
         S2EAndroidWrapper.getSymbolicInt
   an interrupt occurs. After some time, when the scheduler schedules the above method again, 
   the context (registers, processor mode, ...) is not correctly restored which led to a kernel panic. 

I analyze the issue in detail below and propose a fix afterwards.
regards
Andreas Kirchner <akalypse@gmail.com>

#####################################################################################
################################# Bug Explanation ###################################
#####################################################################################

We are in the kernel to handle an interrupt (let's name it: INT1) after a symbolic value is introduced in an application MY_APP. Before the kernel returns to the interrupted application MY_APP , the following two translation blocks TB0 and TB1 from kernel function "ret_to_user" are executed in concrete mode:
TB0:
0xc0023964:  ldr        r1, [sp, #64]
0xc0023968:  ldr        lr, [sp, #60]!
0xc002396c:  msr        SPSR_fsxc, r1 //TB0 and TB1 are split because a modification of PSR causes s->is_jmp = DISAS_UPDATE

TB1:
0xc0023970:  ldmdb      sp, {r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, sl, fp, ip, sp, lr}^
0xc0023974:  nop                        (mov r0,r0)
0xc0023978:  add        sp, sp, #12     ; 0xc
0xc002397c:  movs       pc, lr

The first instruction of TB1 retrieves register values of MY_APP to restore the context. An address which contains a symbolic value is accessed. S2E detects that a symbolic value wants to be accessed from concrete mode at readRamConcreteCheck(...) and switches to KLEE executor.
The field m_startSymbexAtPC is set to the start of TB1 and a longjump brings QEMU back to the start of the cpu_exec(..)-loop. 

The first thing QEMU does when jumping to cpu_exec(..) is to check if an exception is pending.
It is possible that in the meantime an interrupt (INT2) is pending (the flag exception_index or interrupt_request is set).
When this is the case, s2e_do_interrupt(...) is executed and the program counter is overwritten by the exception handler address (0xFFFF0018).

When executeTranslationBlock(...) is called, it finds that member variable of the ExecutionState m_startSymbexAtPC is not equal to the next program counter address. Thus, S2E does not switch to symbolic execution and executes the first TB of the exception handler concretely.

Problem: When INT2 is handled, we never come back to TB1 which is responsible for restoring the context of MY_APP. This is because:
1.  S2E interrupts TB1, drops it and wants to re-execute TB1 in symbolic mode. 
2. The do_interrupt(..) method interrupts the former plans of 1, and stores 0xc0023974 in the link register.
3. What we miss now is at least a proper execution of the instruction at 0xc0023970. I am not even sure if we ever get back to this particular TB1 state. In every case, we should not be interrupted at such a sensible place.
4. The consequence is: When MY_APP is resumed after INT2, TB1 is not (fully?) executed, thus the context is not restored and weird things can happen.

#####################################################################################
########################## How I fixed the Bug?  ####################################
#####################################################################################

I forbid exception handling when a TB wants to be re-executed in symbolic mode.

1. Add a helper method in S2EExecutionState:

    /* Returns true if a TB wants to be re-executed in symbolic mode */
    bool symbexPending() const { return m_startSymbexAtPC != (uint64_t) -1; }

2. Before preparing a switch to an exception handler, check symbexPending():

in cpu-exec.c, I call the helper method to only handle an exception if we have not interrupted 
concrete execution of a TB to switch to symbolic execution in the middle of a TB.

E.g.:

if (interrupt_request && !s2e_is_symbex_pending(g_s2e_state)) {
// ...
	do_interrupt(...);
}

If you don't want to touch cpu-exec.c you can restrict yourself to interrupt exception handling and only modify
S2EExecutor:

void s2e_do_interrupt(struct S2E* s2e, struct S2EExecutionState* state)
{
	if(!state->symbexPending()) {
		s2e->getExecutor()->doInterrupt(state);
	}
}

Maybe there is a better solution to it, but it fixed the problem for my test app.