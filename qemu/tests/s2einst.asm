;Assemble this file to a raw binary, and specify it as the BIOS

zerobuf: times 0x10000 db 0

[org 0xf0000]
start:

;This is the custom instruction
;db 0xf1
;dq 0xbadf00ddeadbeef
;dq 0x0
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
db 0x0f
db 0x3f ; S2EOP

db 0x00 ; Built-in instructions
db 0x01 ; enable s2e
db 0x00
db 0x00

dd 0x0
dq 0x0

db 0x0f
db 0x3f ; s2e prefix
db 0x00 ; build-in opcode
db 0x03 ; insert symbolic value
db 0x00 ; not used
db 0x00 ; not used
dd 0x04  ; size
dq 0x100 ; address

mov eax, [0x100]
add eax, 2

cmp eax, 10
jz branch1

_stop:
;cli
;hlt
inc edx
jmp _stop

branch1:
_stop1:
;cli
;hlt
inc ecx
jmp _stop1

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
