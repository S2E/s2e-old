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
.set symbstart,  0x7200

/* start code */
_start:
	b init_stack


init_stack: @we use full descending mode (and do not care about different execution modes)
	mov sp, #stackstart
	b main

main:
	bl initial_test
	bl test_mem
	B _idle

initial_test:
	stmfd sp!,{r14}
	mov r8, #symbstart
	mov r0, #10
	bl s2e_disable_symbolic
	bl s2e_enable_symbolic
	bl s2e_enable_forking
	adr r0, msg
	bl s2e_message
	bl s2e_version
	mov r5, r0
	ldmfd sp!,{r15}

test_mem:
	stmfd sp!,{r5, r14}
	mov r7, #0
	bl s2e_get_path_id
	mov r5, r0
	bl s2e_get_ram_object_bits
	mov r6, r0
stm0:
	cmp r7,#10
	beq stm1
	sub r8, r8, #4		@decrement (custom symb. value 'stack')
	mov r0, r8			@address of symb. value
	mov r1, #4			@size of symb. value = 4byte
	mov r2, #0			@name is empty (use default name)
    bl s2e_make_symbolic
    ldr r0, [r0,#0]		@get the symbolic value?
    cmp r0, #0
    beq stm1
    add r7, #1
    b stm0
stm1:
	bl s2e_get_path_id
	mov r9, #1
	mov r9, r9, lsl r6	@shift by ram_object_bits to get the size of the page
	mov r0, r9			@expression
	adr r1, val			@name
	bl s2e_print_expression
	ldmfd sp!,{r5, r15}		@lets stop here for now

_idle:
	NOP
	B _idle

msg:
	.asciz "Hello S2E-Android!"
val:
	.asciz "Pagesize: "

version:
	NOP
	NOP

.end

