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
bl s2e_enable_symbolic
bl s2e_disable_symbolic
adr r0, msg 
bl s2e_message
bl s2e_version
@todo: improve code below
adr r1, version
mov r2, r1		  @store start of output
str r0, [r1], #4  @write version
mov r0, #00
strb r0, [r1], #4 @tailing zero byte
mov r0, r2		  @prepare arg0 (adress to version string)
bl s2e_message
NOP
NOP
NOP
NOP
B _idle

_idle:
NOP
B _idle

msg:
.asciz "Hello S2E-Android!"

version:
NOP
NOP
.end

