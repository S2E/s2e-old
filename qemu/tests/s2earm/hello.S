@filename hello.S
.global _start
.text
.arm
/***********************************************************************************
section: a range of address with no gaps.
main sections: text, data, bss.
other: absolute, undefined

text section: contains instructions, constants, ...
data section: similar to text, but alterable.
bss section: hold uninitialized variables, common storage,
absolute section: is unrelocable (address 0 of this section maps to runtime address 0.)

run as with the following option:
-mcpu=arm926e
-march=armv5te

 
************************************************************************************/
.equ testvar, 10 @just for fun
.set stackstart, 	0x7000
.set symbstart,  	0x8000
.set memstart,		0x40000
.set testvalue, 	0xDE

/* start code */
_start:
	b init_stack


init_stack: @we use full descending mode (and do not care about different execution modes)
	mov r0, #0
	mov sp, #stackstart
	mov r8, #symbstart
	b main

main:
@        bl test_flags
@	bl initial_test
	bl test_mem
@	bl test_forking
@	bl switch_main
@	bl register_check
	B _idle

register_check:
	stmfd sp!,{r14}
	bl symbolic_int

	mov r0, #1
	mov r1, #2
	mov r2, #3
	mov r3, #4
	mov r4, #5
	mov r5, #6
	mov r6, #7
	mov r7, #8
	mov r9, #10
	mov r11, #12
	mov r12, #13

	ldr r10, [r8,#0]	@get the symbolic value

    cmp r10, #0			@fork state
    bleq register_check_compare
    blne register_check_compare
    mov r10, #10
    cmp r10, #10						@does not fork
    bne switch_error
    bl s2e_kill_state
    ldmfd sp!,{r15}

register_check_compare:
	stmfd sp!,{r14}
	cmp r0, #1
	bne switch_error
	cmp r1, #2
	bne switch_error
	cmp r2, #3
	bne switch_error
	cmp r3, #4
	bne switch_error
	cmp r4, #5
	bne switch_error
	cmp r5, #6
	bne switch_error
	cmp r6, #7
	bne switch_error
	cmp r7, #8
	bne switch_error
	cmp r9, #10
	bne switch_error
	cmp r11, #12
	bne switch_error
	cmp r12, #13
	bne switch_error
    ldmfd sp!,{r15}

switch_main:
	stmfd sp!,{r14}
	sub r8, r8, #4		@decrement (custom symb. value 'stack')

	mov r0, r8			@address of symb. value
	mov r1, #4			@size of symb. value = 4byte
	mov r2, #0			@name is empty (use default name)
    bl s2e_make_symbolic
	mov r4, #memstart

    ldr r10, [r8,#0]	@get the symbolic value
    cmp r10, #0
    movne r0, #163     @should fork into 2 states
  	mov r6, #6
switch_loop:
	cmp r6, #6
	bne switch_error
	b switch_loop
    ldmfd sp!,{r15}

switch_error:
	mov r0, r6
	adr r1, statebad
	bl s2e_kill_state

test_forking:
	stmfd sp!,{r14}
	sub r8, r8, #4		@decrement (custom symb. value 'stack')

	mov r0, r8			@address of symb. value
	mov r1, #4			@size of symb. value = 4byte
	mov r2, #0			@name is empty (use default name)
    bl s2e_make_symbolic
	mov r4, #memstart

    ldr r10, [r8,#0]	@get the symbolic value
    cmp r10, #0
    movge r0, #163     @should fork into 2 states

	bl s2e_get_path_id
	mov r5, r0

	adr r1, labelreg5
	bl s2e_print_expression


	str r5, [r4],#+4
	str r5, [r4],#+4
    bl compareMem


	adr r1, symbtest
	bl s2e_kill_state
    ldmfd sp!,{r15}

compareMem:
	stmfd sp!,{r0, r14}
	sub r4, r4, #8

	mov r0, r4
	adr r1, adrafter
	bl s2e_print_expression


	ldr r0, [r4],#+4
	cmp r0, r5
	bne sterr  @if not, goto error
	bl checkpoint
	ldr r0, [r4],#+4
	cmp r0, r5
	bne sterr  @if not, goto error
	ldmfd sp!,{r0, r15}

test_flags:
	stmfd sp!,{r14}
	msr cpsr_f, 0xF0000000 @set all flags to 1
	bl print_status
	msr cpsr_f, 0x00000000 @set all flags to 0
	bl print_status
	mov r0, #1
	cmp r0, #1	
	bl print_status	
	subs r0, r0 @z should be enabled
	bl print_status
	mov r0, #0
	subs r0, r0, #10 @should enable n
	bl print_status
	msr cpsr_f, 0x00000000 @set all flags to 0
	bl print_status
	ldmfd sp!,{r15}

print_status:
	stmfd sp!,{r14}
	mov r1, #4160749568  @mask 0xF8000000
	mrs r0, cpsr
	and r0, r0, r1
	adr r1, flagtest
	bl s2e_print_expression
	ldmfd sp!,{r15}
initial_test:
	stmfd sp!,{r14}
	mov r0, #10
	bl s2e_disable_symbolic
	bl s2e_enable_symbolic
	bl s2e_disable_forking
	bl s2e_enable_forking
	adr r0, msg
	bl s2e_message
	bl s2e_version
	mov r5, r0
	ldmfd sp!,{r15}

checkpoint:
	stmfd sp!,{r14}
	adr r0, checkmsg
	bl s2e_message
	ldmfd sp!,{r15}

test_mem:
   @
	@registers with special roles:
	@   r4  ... contains the current address to store/check
	@   r5  ... contains path id of current path
	@   r6  ... contains ram object bits (to build pagesize)
	@   r7  ... used as a counter to limit the loop which forks the state
	@   r8  ... custom stack where symbolic data is written (has to be decremented by <size of last written symbolic value> before use)
	@   r9  ... contains pagesize
	@
	stmfd sp!,{r5, r14}
	mov r7, #0
	bl s2e_get_ram_object_bits
	mov r6, r0
	mov r9, #1
	mov r9, r9, lsl r6	@shift by ram_object_bits to get the size of the page
stm0:
	mov r0, r7		    @print counter
	adr r1, cnt		@name
	bl s2e_print_expression

	cmp r7,#1     @the number represents the number of cycles in stm0
	bge stm1

	mov r10, #0
@ comment the following two lines to have single-state memory check /uncomment for  multi-state-memory-check
	bl symbolic_int
    ldr r10, [r8,#0]	@get the symbolic value

    cmp r10, #0			@fork state
    beq stm1			@one state enters stm1, the other continues
    add r7, r7, #1
    b stm0

symbolic_int:
	stmfd sp!,{r14}
	sub r8, r8, #4		@decrement (custom symb. value 'stack')

	mov r0, r8			@address of symb. value
	mov r1, #4			@size of symb. value = 4byte
	mov r2, #0			@name is empty (use default name)
    bl s2e_make_symbolic

    mov r0, r8
    mov r1, #4
    mov r2, #0
    bl s2e_print_memory
    ldmfd sp!,{r15}

stm1:
	bl s2e_get_path_id
	mov r5, r0
	add r5, #10	

	mov r0, r9		@expression argument (r9 contains pagesize)
	adr r1, val			@name
	bl s2e_print_expression
	mov r4, #1048576 @ starting memory address 0x100000
stm2_restart:
	adr r1, adrafter
	mov r10, r9		@(re)arm counter
stm2:
	@Fill page with test pattern (path id stored in r5)

	str r5, [r4],#+4

	sub r10, r10, #1 @decrease counter
	cmp r10, #0
	bne stm2

	mov r10, r9 @reset counter
	mov r10, r10, lsl #2
	sub r4, r4, r10 @get back to memory starting address

	mov r10, r9 @reset counter
	adr r1, mem			@name param2 for print expression
stm3:
	@Check if the memory is correct
	ldr r0, [r4],#+4
	cmp r0, r5 @do we get the path id back?
	bne sterr  @if not, goto error
	sub r10, r10, #1
	cmp r10, #0
	bne stm3

	mov r0, r4
	bl s2e_print_expression
	cmp r4, #16777216 @ 0x1000000
	blt stm2_restart

	@successfully completed mem test
	mov r0, #0
	adr r1, memok
	bl s2e_kill_state
	bl _idle
	ldmfd sp!,{r5, r15}

_idle:
	NOP
	B _idle

sterr:
	adr r1, membadval
	bl s2e_print_expression
	mov r0, r5
	adr r1, membad
	bl s2e_kill_state
	bl _idle

msg:.asciz "Hello S2E-Android!"
val:		.asciz "Pagesize"
.align 2
mem:		.asciz "Memcheck"
.align 2
symbtest:	.asciz "SymbTest"
.align 2
flagtest:	.asciz "Flagtest"
.align 2
memok: 		.asciz "Memory test passed ok"
.align 2
membad: 	.asciz "MEMORY TEST FAILED"
.align 2
membadval: 	.asciz "Bad value"
.align 2
cnt:		.asciz "R7 counter"
.align 2
adrafter: 	.asciz "address"
.align 2
labelreg5:	.asciz "Testval"
.align 2
checkmsg:	.asciz "Checkpoint"
.align 2
statebad:	.asciz "State Switch Check FAILED"
.end

