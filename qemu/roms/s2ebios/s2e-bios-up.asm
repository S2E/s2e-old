;Assemble this file to a raw binary, and specify it as the BIOS
;This binary will be loaded at 0xf0000

org 0

%include "init.asm"

[bits 16]
start:
    cli
    mov ax, cs
    mov ds, ax
    mov ax, 0x8000
    mov ss, ax
    mov sp, 0
    call init_pmode

    cli
    hlt


times 0x10000 - 16 - ($-$$) db 0

;0xf000:fff0
boot:
jmp 0xf000:start

times 0x10000-($-$$) db 0
