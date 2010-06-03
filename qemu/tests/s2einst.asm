;Assemble this file to a raw binary, and specify it as the BIOS

org 0xe0000
zerobuf: times 0x10000 db 0



;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
[bits 32]
s2e_timing:
push ebp
mov ebp, esp

mov eax, [ebp - 4]
mov edx, [ebp - 8]

db 0x0f
db 0x3f ; S2EOP

db 0x00 ; Built-in instructions
db 0x04 ; Insert timing
db 0x00
db 0x00
dd 0x0

leave
ret

s2e_get_path_id:
push ebp
mov ebp, esp

db 0x0f
db 0x3f ; S2EOP

db 0x00 ; Built-in instructions
db 0x04 ; Get path id
db 0x00
db 0x00
dd 0x0

leave
ret

s2e_enable:
push ebp
mov ebp, esp

db 0x0f
db 0x3f ; S2EOP

db 0x00 ; Built-in instructions
db 0x01 ; Enable symbex
db 0x00
db 0x00
dd 0x0

leave
ret

s2e_disable:
push ebp
mov ebp, esp

db 0x0f
db 0x3f ; S2EOP

db 0x00 ; Built-in instructions
db 0x02 ; Disable symbex
db 0x00
db 0x00
dd 0x0

leave
ret

s2e_make_symbolic:
push ebp
mov ebp, esp
push ebx

mov eax, [ebp + 0x8] ;address
mov ebx, [ebp + 0xC] ;size
mov ecx, [ebp + 0x10] ;asciiz

db 0x0f
db 0x3f ; S2EOP

db 0x00 ; Built-in instructions
db 0x03 ; Make symbolic
db 0x00
db 0x00
dd 0x0

pop ebx
leave
ret


s2e_kill_state:
push ebp
mov ebp, esp
push ebx

db 0x0f
db 0x3f ; S2EOP

db 0x00 ; Built-in instructions
db 0x06 ; Kill the current state
db 0x00
db 0x00
dd 0x0

pop ebx
leave
ret


s2e_print_expression:
push ebp
mov ebp, esp

mov eax, [ebp + 0x8] ;expression
mov ecx, [ebp + 0xC] ;message

db 0x0f
db 0x3f ; S2EOP

db 0x00 ; Built-in instructions
db 0x07 ; print expression
db 0x00
db 0x00
dd 0x0

leave
ret

s2e_print_memory:
push ebp
mov ebp, esp
push ebx

mov eax, [ebp + 0x8] ;addr
mov ebx, [ebp + 0xC] ;size
mov ecx, [ebp + 0xC] ;message

db 0x0f
db 0x3f ; S2EOP

db 0x00 ; Built-in instructions
db 0x08 ; print memory
db 0x00
db 0x00
dd 0x0

pop ebx
leave
ret

s2e_int:
push ebp
mov ebp, esp
sub esp, 4

push 0
push 4
lea eax, [ebp-4]
push eax
call s2e_make_symbolic
add esp, 4*3
mov eax, [ebp-4]

leave
ret

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
pm_gdtraddr	dd pm_nullseg

init_pmode:

    cli
    lgdt [pm_gdtr]
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    db 66h
    db 67h
    db 0xEA
    dd s2e_test
    dw OSCODE32_SEL
    hlt

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;S2E test - this runs in protected mode
[bits 32]
s2e_test:

    mov eax, OSDATA32_SEL
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov esp, 0x80000

    call s2e_enable

    mov ecx, 0


a:
    push ecx
    call s2e_int
    pop ecx

    ;;Print the counter
    push eax
    push ecx

    push 0
    push ecx
    call s2e_print_expression
    add esp, 8

    pop ecx
    pop eax

    ;;Print the symbolic value
    push eax
    push ecx

    push 0
    push eax
    call s2e_print_expression
    add esp, 8

    pop ecx
    pop eax

    test eax, 1
    jz exit1    ; if (i < symb) exit
    inc ecx
    jmp a

    call s2e_disable
    call s2e_kill_state

    exit1:
    call s2e_disable
    call s2e_kill_state


    hlt

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
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





;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

times 0x20000 - 16 - ($-$$) db 0

;0xf000:fff0
boot:
jmp 0xf000:start

times 0x20000-($-$$) db 0
