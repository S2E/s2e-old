segment .text
global bit_scan_forward_64
global bit_scan_forward_64_posix
global s2e_setjmp_win32
global s2e_longjmp_win32
global s2e_setjmp_posix
global s2e_longjmp_posix
global _s2e_setjmp_posix
global _s2e_longjmp_posix


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

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;rcx: pointer to jmp_buf
s2e_setjmp_win32:
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
    mov [rcx + s2e_jmpbuf._r15], r15
    mov rax, [rsp]
    mov [rcx + s2e_jmpbuf._rip], rax
    xor rax, rax
    ret

;rcx: pointer to jmp_buf
;rdx: value
s2e_longjmp_win32:
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

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;


;rdi: pointer to jmp_buf
s2e_setjmp_posix:
    mov [rdi + s2e_jmpbuf._rax], rax
    mov [rdi + s2e_jmpbuf._rbx], rbx
    mov [rdi + s2e_jmpbuf._rcx], rcx
    mov [rdi + s2e_jmpbuf._rdx], rdx
    mov [rdi + s2e_jmpbuf._rsi], rsi
    mov [rdi + s2e_jmpbuf._rdi], rdi
    mov [rdi + s2e_jmpbuf._rbp], rbp
    mov [rdi + s2e_jmpbuf._rsp], rsp
    mov [rdi + s2e_jmpbuf._r8], r8
    mov [rdi + s2e_jmpbuf._r9], r9
    mov [rdi + s2e_jmpbuf._r10], r10
    mov [rdi + s2e_jmpbuf._r11], r11
    mov [rdi + s2e_jmpbuf._r12], r12
    mov [rdi + s2e_jmpbuf._r13], r13
    mov [rdi + s2e_jmpbuf._r14], r14
    mov [rdi + s2e_jmpbuf._r15], r15
    mov rax, [rsp]
    mov [rdi + s2e_jmpbuf._rip], rax
    xor rax, rax
    ret

;rdi: pointer to jmp_buf
;rsi: value
s2e_longjmp_posix:
    mov rax, [rdi + s2e_jmpbuf._rax]
    mov rbx, [rdi + s2e_jmpbuf._rbx]
    mov rcx, [rdi + s2e_jmpbuf._rcx]
    mov rdx, [rdi + s2e_jmpbuf._rdx]
    ;mov rsi, [rdi + s2e_jmpbuf._rsi]
    mov rdi, [rdi + s2e_jmpbuf._rdi]
    mov rbp, [rdi + s2e_jmpbuf._rbp]
    mov rsp, [rdi + s2e_jmpbuf._rsp]
    mov r8, [rdi + s2e_jmpbuf._r8]
    mov r9, [rdi + s2e_jmpbuf._r9]
    mov r10, [rdi + s2e_jmpbuf._r10]
    mov r11, [rdi + s2e_jmpbuf._r11]
    mov r12, [rdi + s2e_jmpbuf._r12]
    mov r13, [rdi + s2e_jmpbuf._r13]
    mov r14, [rdi + s2e_jmpbuf._r14]
    mov r15, [rdi + s2e_jmpbuf._r15]
    mov rax, [rdi + s2e_jmpbuf._rip]
    mov [rsp], rax
    mov rax, 1
    cmp rsi, 0
    cmovnz rax, rsi
    ret

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;


;rdi: pointer to jmp_buf
_s2e_setjmp_posix:
    mov [rdi + s2e_jmpbuf._rax], rax
    mov [rdi + s2e_jmpbuf._rbx], rbx
    mov [rdi + s2e_jmpbuf._rcx], rcx
    mov [rdi + s2e_jmpbuf._rdx], rdx
    mov [rdi + s2e_jmpbuf._rsi], rsi
    mov [rdi + s2e_jmpbuf._rdi], rdi
    mov [rdi + s2e_jmpbuf._rbp], rbp
    mov [rdi + s2e_jmpbuf._rsp], rsp
    mov [rdi + s2e_jmpbuf._r8], r8
    mov [rdi + s2e_jmpbuf._r9], r9
    mov [rdi + s2e_jmpbuf._r10], r10
    mov [rdi + s2e_jmpbuf._r11], r11
    mov [rdi + s2e_jmpbuf._r12], r12
    mov [rdi + s2e_jmpbuf._r13], r13
    mov [rdi + s2e_jmpbuf._r14], r14
    mov [rdi + s2e_jmpbuf._r15], r15
    mov rax, [rsp]
    mov [rdi + s2e_jmpbuf._rip], rax
    xor rax, rax
    ret

;rdi: pointer to jmp_buf
;rsi: value
_s2e_longjmp_posix:
    mov rax, [rdi + s2e_jmpbuf._rax]
    mov rbx, [rdi + s2e_jmpbuf._rbx]
    mov rcx, [rdi + s2e_jmpbuf._rcx]
    mov rdx, [rdi + s2e_jmpbuf._rdx]
    ;mov rsi, [rdi + s2e_jmpbuf._rsi]
    mov rdi, [rdi + s2e_jmpbuf._rdi]
    mov rbp, [rdi + s2e_jmpbuf._rbp]
    mov rsp, [rdi + s2e_jmpbuf._rsp]
    mov r8, [rdi + s2e_jmpbuf._r8]
    mov r9, [rdi + s2e_jmpbuf._r9]
    mov r10, [rdi + s2e_jmpbuf._r10]
    mov r11, [rdi + s2e_jmpbuf._r11]
    mov r12, [rdi + s2e_jmpbuf._r12]
    mov r13, [rdi + s2e_jmpbuf._r13]
    mov r14, [rdi + s2e_jmpbuf._r14]
    mov r15, [rdi + s2e_jmpbuf._r15]
    mov rax, [rdi + s2e_jmpbuf._rip]
    mov [rsp], rax
    mov rax, 1
    cmp rsi, 0
    cmovnz rax, rsi
    ret


