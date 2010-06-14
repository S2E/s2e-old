;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
[bits 16]

;Quick and dirty pmode init
%define SEGMENT_COUNT 3
%define OSDATA32_SEL  0x08
%define OSCODE32_SEL  0x10

pm_nullseg:
        dd 0
        dd 0
pm_dataseg:
        dw 0xFFFF	;Seg limit
        dw 0x0000	;Base
        db 0x0	;base
        db 0x80 + 0x10 + 2 ;Present+code or data + RW data
        db 0x80 + 0x40 + 0xF; Granularity+32bits + limit
        db 0

pm_codeseg:
        dw 0xFFFF	;Seg limit
        dw 0	;Base
        db 0x0	;base
        db 0x80 + 0x10 + 10 ;Present+code or data + Exec/RO
        db 0x80 + 0x40 + 0xF; Granularity+32bits + limit
        db 0

;GDTR value
pm_gdtr:
        dw 0x8*SEGMENT_COUNT
pm_gdtraddr	dd 0xF0000 + pm_nullseg

init_pmode:

    cli
    lgdt [pm_gdtr]
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    db 66h
    db 67h
    db 0xEA
    dd init_pmode2+0xf0000
    dw OSCODE32_SEL
    hlt

[bits 32]
init_pmode2:
    mov eax, OSDATA32_SEL
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov esp, 0x80000
    mov eax, 0xe0000
    call eax
    cli    
    hlt





