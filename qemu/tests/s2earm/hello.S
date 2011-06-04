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
.set stackstart, 0x7000
.set symbstart,  0x8000

/* start code */
_start:
	b init_stack


init_stack: @we use full descending mode (and do not care about different execution modes)
	mov sp, #stackstart
	mov r8, #symbstart
	b main

main:
@    bl test_flags
@	bl initial_test
	bl test_mem
@	bl test_forking
	B _idle

test_forking:
	stmfd sp!,{r14}
	sub r8, r8, #4		@decrement (custom symb. value 'stack')

	mov r0, r8			@address of symb. value
	mov r1, #4			@size of symb. value = 4byte
	mov r2, #0			@name is empty (use default name)
    bl s2e_make_symbolic

    ldr r10, [r8,#0]	@get the symbolic value

    cmp r10, #0
    beq test_mem			@forking should occur
    ldmfd sp!,{r15}

test_flags:
	stmfd sp!,{r14}
	msr cpsr_f, 0xF0000000 @set all flags to 1
	bl print_status
	msr cpsr_f, 0x00000000 @set all flags to 0
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

	cmp r7,#10
	bl print_status
	beq stm1

	sub r8, r8, #4		@decrement (custom symb. value 'stack')

	mov r0, r8			@address of symb. value
	mov r1, #4			@size of symb. value = 4byte
	mov r2, #0			@name is empty (use default name)
    bl s2e_make_symbolic

    mov r0, r8
    mov r1, #4
    mov r2, #0
    bl s2e_print_memory

    ldr r10, [r8,#0]	@get the symbolic value
    cmp r10, #0			@fork state
    beq stm1			@one state enters stm1, the other continues
    add r7, r7, #1
    b stm0
stm1:
	bl s2e_get_path_id
	mov r5, r0

	mov r0, r9		@expression argument (r9 contains pagesize)
	adr r1, val			@name
	bl s2e_print_expression
	mov r4, #1048576 @ starting memory address 0x100000
stm2_restart:
	adr r1, adrafter
	mov r0, r4
	bl s2e_print_expression
	mov r10, r9		@(re)arm counter
stm2:
	@Fill page with test pattern (path id stored in r5)

	str r5, [r4],#+4

	sub r10, r10, #1 @decrease counter
	cmp r10, #0
	bne stm2
	@sub r4, r4, #4 @one step back - last address is not written
	mov r10, r9 @reset counter
	mov r10, r10, lsl #2

	mov r0, r10		@expression argument
	bl s2e_print_expression

	sub r4, r4, r10 @get back to memory starting address

	mov r10, r9 @reset counter
	adr r1, mem			@name param2 for print expression
stm3:
	@Check that the memory is correct
	ldr r0, [r4],#+4
	cmp r0, r5 @do we get the path id back?
	bne sterr  @if not, goto error
	mov r0, r4
	bl s2e_print_expression
	sub r10, r10, #1
	cmp r10, #0
	bne stm3

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
mov r0, r4
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
adrafter: 	.asciz "R4 address "


.end

