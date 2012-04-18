


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
    db 0x05 ; Get path id
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

s2e_fork_enable:
    push ebp
    mov ebp, esp

    db 0x0f
    db 0x3f ; S2EOP

    db 0x00 ; Built-in instructions
    db 0x09 ; Enable forking
    db 0x00
    db 0x00
    dd 0x0

    leave
    ret

s2e_fork_disable:
    push ebp
    mov ebp, esp

    db 0x0f
    db 0x3f ; S2EOP

    db 0x00 ; Built-in instructions
    db 0x0a ; Disable forking
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

s2e_make_concolic:
    push ebp
    mov ebp, esp
    push ebx

    mov eax, [ebp + 0x8] ;address
    mov ebx, [ebp + 0xC] ;size
    mov ecx, [ebp + 0x10] ;asciiz

    db 0x0f
    db 0x3f ; S2EOP

    db 0x00 ; Built-in instructions
    db 0x11 ; Make concolic
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

    mov ebx, [ebp + 0xC] ;message
    mov eax, [ebp + 0x8] ;status

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

s2e_get_ram_object_bits:
    db 0x0f
    db 0x3f ; S2EOP

    db 0x00 ; Built-in instructions
    db 0x52 ; s2e_get_ram_object_bits
    db 0x00
    db 0x00
    dd 0x0

    ret

s2e_print_memory:
    push ebp
    mov ebp, esp
    push ebx

    mov eax, [ebp + 0x8] ;addr
    mov ebx, [ebp + 0xC] ;size
    mov ecx, [ebp + 0x10] ;message

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

s2e_concolic_int:
    push ebp
    mov ebp, esp
    sub esp, 4

    ;Initialize the memory location
    ;we want to make symbolic with
    ;the passed concrete value
    mov eax, [ebp + 8]
    mov [ebp - 4], eax

    push 0
    push 4
    lea eax, [ebp-4]
    push eax
    call s2e_make_concolic
    add esp, 4*3
    mov eax, [ebp-4]

    leave
ret

s2e_print_message:
    push ebp
    mov ebp, esp

    mov eax, [ebp+8] ; message

    db 0x0f
    db 0x3f ; S2EOP

    db 0x00 ; Built-in instructions
    db 0x10 ; print message
    db 0x00
    db 0x00
    dd 0x0


    leave
    ret


s2e_get_state_count:
    db 0x0f
    db 0x3f ; S2EOP

    db 0x00 ; Built-in instructions
    db 0x30 ; get number of active states
    db 0x00 ; succeed
    db 0x00
    dd 0x00
    ret

s2e_get_proc_count:
    db 0x0f
    db 0x3f ; S2EOP

    db 0x00 ; Built-in instructions
    db 0x31 ; get number of current s2e instances
    db 0x00 ; succeed
    db 0x00
    dd 0x00
    ret

s2e_sleep:
    mov eax, [esp + 4];

    db 0x0f
    db 0x3f ; S2EOP

    db 0x00 ; Built-in instructions
    db 0x32 ; sleep x seconds
    db 0x00 ; succeed
    db 0x00
    dd 0x00
    ret

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Raw Plugin Custom Instructions
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

s2e_raw_load_import:
    push ebp
    mov ebp, esp

    mov eax, [ebp + 0x8] ;dllname
    mov ebx, [ebp + 0xC] ;funcName
    mov ecx, [ebp + 0x10] ;functionptr

    db 0x0f
    db 0x3f ; S2EOP

    db 0x00 ; Built-in instructions
    db 0xAA ; raw monitor
    db 0x01 ; load imports
    db 0x00
    dd 0x0

    leave
    ret


s2e_raw_load_module:
    push ebp
    mov ebp, esp

    mov eax, [ebp + 0x8] ;name
    mov ebx, [ebp + 0xC] ;loadbase
    mov ecx, [ebp + 0x10] ;size

    db 0x0f
    db 0x3f ; S2EOP

    db 0x00 ; Built-in instructions
    db 0xAA ; raw monitor
    db 0x00 ; load module
    db 0x00
    dd 0x0

    leave
    ret

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Cooperative Scheduler Custom Instructions
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

s2e_coop_next:
    push ebp
    mov ebp, esp

    mov eax, [ebp + 0x8] ;state id

    db 0x0f
    db 0x3f ; S2EOP

    db 0x00 ; Built-in instructions
    db 0xAB ; coop scheduler
    db 0x00 ; schedule next
    db 0x00
    dd 0x0

    leave
    ret

s2e_coop_yield:
    push ebp
    mov ebp, esp

    db 0x0f
    db 0x3f ; S2EOP

    db 0x00 ; Built-in instructions
    db 0xAB ; coop scheduler
    db 0x01 ; yield
    db 0x00
    dd 0x0

    leave
    ret

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; State Manager Custom Instructions
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

s2e_sm_succeed:
    db 0x0f
    db 0x3f ; S2EOP

    db 0x00 ; Built-in instructions
    db 0xAD ; state manager
    db 0x00 ; succeed
    db 0x00
    dd 0x00
    ret

s2e_sm_succ_count:
    db 0x0f
    db 0x3f ; S2EOP

    db 0x00 ; Built-in instructions
    db 0xAD ; state manager
    db 0x01 ; count of succeeded states across all nodes
    db 0x00
    dd 0x00
    ret
