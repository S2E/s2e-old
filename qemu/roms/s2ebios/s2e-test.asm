

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;S2E test - this runs in protected mode
[bits 32]
s2e_test:
    ;jmp s2e_test
    ;call s2e_test2
    call s2e_isa_dev
    jmp s2e_test
    cli
    hlt


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;Testing symbolic ISA devices
s2e_isa_dev:
    mov dx, 0x100
    in  ax, dx
    out dx, ax
    ret

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;Testing complicated symbolic expressions
s2e_simplifier1:
    call s2e_enable
    call s2e_int

    ;Testing chains of Zexts
    mov bl, al
    movzx cx, bl
    movzx edx, cx
    cmp edx, 1
    jae ss1

ss1:

    call s2e_disable
    call s2e_kill_state

    ret



;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;Testing symbolic memory

s2e_symbmem1:
    mov ebx, [0]
    call s2e_enable
    call s2e_int
    cmp dword [eax], 0x1000
    jae sm1
    mov ebx, [eax]
    sm1:
    call s2e_disable
    call s2e_kill_state
ret


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
msg_s2e_int1: db "Called the test handler", 0

s2e_test_int_hdlr:
    push int_msg
    push 0x80
    call s2e_print_expression
    add esp, 8


    ;Access the eflags register
    call s2e_int
    mov edi, [esp + 4]
    test eax, esi

    push 0
    push edi
    call s2e_print_expression
    add esp, 8

    iret

s2e_test_int1:
    ;Register the test interrupt handler
    mov edi, s2e_test_int_hdlr
    mov eax, 0x80
    call add_idt_desc

    ;Starts symbexec
    call s2e_enable
    call s2e_int
    cmp eax, 0
    jz sti_1
    int 0x80
sti_1:
    int 0x80
    call s2e_disable
    call s2e_kill_state
    ret

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;Infinite loop, without state killing
s2e_test2:
    call s2e_enable

s2etest2_1:
    call s2e_int
    cmp eax, 0
    jz __a
__a:
    jmp s2etest2_1

    ret

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;Loop that creates symbolic values on each iteration
s2e_test1:

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


    ret
