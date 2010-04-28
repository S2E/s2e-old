;Assemble this file to a raw binary, and specify it as the BIOS

zerobuf: times 0x10000 db 0

[org 0xf0000]
start:

;This is the custom instruction
db 0xf1
dq 0xbadf00ddeadbeef
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
db 0xf1 ; S2EOP
db 0x00 ; Built-in instructions
db 0x01 ; enable s2e
dw 0x00
dd 0x0


db 0xf1
db 0x00
db 0x03 ; make register symbolic
db 0x00 ; register eax
db 0x08 ; register size
dq 0x00

mov ebx, eax
add ebx, 2


db 0xf1
db 0x00
db 0x02 ; disable s2e
dw 0x00
dd 0x00

cli
hlt

times 0x20000 - 16 - ($-$$) db 0

;0xf000:fff0
boot:
jmp 0xf000:start

times 0x20000-($-$$) db 0
