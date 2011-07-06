[bits 32]

msg_sm: db "FAILED - Must have only one state after the barrier", 0
msg_sminst: db "FAILED - Must have only one instance after the barrier", 0
msg_ok: db "SUCCESS", 0

s2e_sm_test:
    call s2e_sm_fork

    call s2e_sm_succeed

    ;Wait for the state to stabilize
    ;(it takes a while for all processes to be killed)
    push 3
    call s2e_sleep
    add esp, 4

    ;At this point, there can be only one state
    call s2e_get_state_count
    cmp eax, 1
    je sst1

    ;We must have only one state at this point
    push msg_sm
    push eax
    call s2e_kill_state

sst1:

    ;We must have only one S2E instance
    call s2e_get_proc_count
    cmp eax, 1
    je sst2

    push msg_sminst
    push eax
    call s2e_kill_state

sst2:

    ;Finish the test
    push msg_ok
    push 0
    call s2e_kill_state
    add esp, 8
    ret


;Fork lots of states
s2e_sm_fork:
    push ebp
    mov ebp, esp
    sub esp, 4

    call s2e_fork_enable

    mov dword [ebp - 4], 4  ; Set forking depth to 4 (2^^4 states)
ssf1:
    call s2e_int
    cmp eax, 0
    ja ssf2
ssf2:
    dec dword [ebp - 4]; Decrement forking depth
    jnz ssf1

    leave
    ret

