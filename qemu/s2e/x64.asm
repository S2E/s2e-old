segment .text
global _bit_scan_forward_64
global bit_scan_forward_64_posix

[bits 64]

;int bit_scan_forward_64(uint64_t *SetIndex, uint64_t Mask);
;RCX first parameter, RDX second parameter
_bit_scan_forward_64:
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
