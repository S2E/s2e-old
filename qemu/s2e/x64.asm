segment .text
global _bit_scan_forward_64

[bits 64]

;int bit_scan_forward_64(uint64_t *SetIndex, uint64_t Mask);
;RCX first parameter, RDX second parameter
_bit_scan_forward_64:
    xor rax, rax
    bsf rdx, rdx
    mov [rcx], rdx
    setnz al
    ret
