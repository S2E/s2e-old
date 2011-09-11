segment .text
global bit_scan_forward_64
global bit_scan_forward_64_posix
global s2e_setjmp
global s2e_longjmp


[bits 64]

;int bit_scan_forward_64(uint64_t *SetIndex, uint64_t Mask);
;RCX first parameter, RDX second parameter
bit_scan_forward_64:
    xor rax, rax
    bsf rdx, rdx
    mov [rcx], rdx
    setnz al
    ret


bit_scan_forward_64_posix:
    xor rax, rax
    bsf rsi, rsi
    mov [rdi], rsi
    setnz al
    ret


struc s2e_jmpbuf
    ._rax:  resq 1
    ._rbx:  resq 1
    ._rcx:  resq 1
    ._rdx:  resq 1
    ._rsi:  resq 1
    ._rdi:  resq 1
    ._rbp:  resq 1
    ._rsp:  resq 1

    ._r8:  resq 1
    ._r9:  resq 1
    ._r10:  resq 1
    ._r11:  resq 1
    ._r12:  resq 1
    ._r13:  resq 1
    ._r14:  resq 1
    ._r15:  resq 1
    ._rip:  resq 1
endstruc


;rcx: pointer to jmp_buf
s2e_setjmp:
    mov [rcx + s2e_jmpbuf._rax], rax
    mov [rcx + s2e_jmpbuf._rbx], rbx
    mov [rcx + s2e_jmpbuf._rcx], rcx
    mov [rcx + s2e_jmpbuf._rdx], rdx
    mov [rcx + s2e_jmpbuf._rsi], rsi
    mov [rcx + s2e_jmpbuf._rdi], rdi
    mov [rcx + s2e_jmpbuf._rbp], rbp
    mov [rcx + s2e_jmpbuf._rsp], rsp
    mov [rcx + s2e_jmpbuf._r8], r8
    mov [rcx + s2e_jmpbuf._r9], r9
    mov [rcx + s2e_jmpbuf._r10], r10
    mov [rcx + s2e_jmpbuf._r11], r11
    mov [rcx + s2e_jmpbuf._r12], r12
    mov [rcx + s2e_jmpbuf._r13], r13
    mov [rcx + s2e_jmpbuf._r14], r14
    mov rax, [rsp]
    mov [rcx + s2e_jmpbuf._rip], rax
    xor rax, rax
    ret

;rcx: pointer to jmp_buf
;rdx: value
s2e_longjmp:
    mov rax, [rcx + s2e_jmpbuf._rax]
    mov rbx, [rcx + s2e_jmpbuf._rbx]
    mov rcx, [rcx + s2e_jmpbuf._rcx]
    ;mov rdx, [rcx + s2e_jmpbuf._rdx]
    mov rsi, [rcx + s2e_jmpbuf._rsi]
    mov rdi, [rcx + s2e_jmpbuf._rdi]
    mov rbp, [rcx + s2e_jmpbuf._rbp]
    mov rsp, [rcx + s2e_jmpbuf._rsp]
    mov r8, [rcx + s2e_jmpbuf._r8]
    mov r9, [rcx + s2e_jmpbuf._r9]
    mov r10, [rcx + s2e_jmpbuf._r10]
    mov r11, [rcx + s2e_jmpbuf._r11]
    mov r12, [rcx + s2e_jmpbuf._r12]
    mov r13, [rcx + s2e_jmpbuf._r13]
    mov r14, [rcx + s2e_jmpbuf._r14]
    mov r15, [rcx + s2e_jmpbuf._r15]
    mov rax, [rcx + s2e_jmpbuf._rip]
    mov [rsp], rax
    mov rax, 1
    cmp rdx, 0
    cmovnz rax, rdx
    ret

