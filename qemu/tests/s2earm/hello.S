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
.set test2, 0xF00 @magic
.text 1

/* start code */
_start:
mov r0, #10
.word 0xFF010000 @S2E enable Symbolic Execution 
.align 2
.word 0xFF020000 @S2E disable Symbolic Execution

adr r0, msg 
.byte 0xFF, 0x10, 0x00, 0x00 @S2E print message 

msg:
.asciz "Hello S2E-Android!"

NOP
NOP
NOP
NOP
