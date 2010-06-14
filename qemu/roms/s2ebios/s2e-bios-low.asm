;This is the lower part of the bios, at 0xe0000
;It runs in protected mode

[bits 32]

org 0xe0000

jmp s2e_test

%include "s2e-inst.asm"
%include "s2e-test.asm"

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;


times 0x10000 - ($-$$) db 0

