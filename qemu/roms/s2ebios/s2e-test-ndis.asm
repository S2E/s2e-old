
struc NDIS_MINIPORT_CHARACTERISTICS32

    MajorNdisVersion:           resb 1
    MinorNdisVersion:           resb 1
    Padding:                    resw 1
    Reserved:                   resd 1
    CheckForHangHandler:        resd 1
    DisableInterruptHandler:    resd 1
    EnableInterruptHandler:     resd 1
    HaltHandler:                resd 1
    HandleInterruptHandler:     resd 1
    InitializeHandler:          resd 1
    ISRHandler:                 resd 1
    QueryInformationHandler:    resd 1
    ReconfigureHandler:         resd 1
    ResetHandler:               resd 1
    SendHandler:                resd 1
    SetInformationHandler:      resd 1
    TransferDataHandler:        resd 1

    ReturnPacketHandler:        resd 1
    SendPacketsHandler:         resd 1
    AllocateCompleteHandler:    resd 1

endstruc


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
NdisMRegisterMiniport:
mov eax, 0
ret 12

NdisAllocateMemory:
mov eax, 0
ret 0x10

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
DrvStruc:
    istruc NDIS_MINIPORT_CHARACTERISTICS32
        at MajorNdisVersion,            db        5
        at MinorNdisVersion,            db        0
        at Padding,                     dw        0
        at Reserved,                    dd        0
        at CheckForHangHandler,         dd        0
        at DisableInterruptHandler,     dd        0
        at EnableInterruptHandler,      dd        0
        at HaltHandler,                 dd        0
        at HandleInterruptHandler,      dd        0
        at InitializeHandler,           dd        NdisInitializeHandler
        at ISRHandler,                  dd        0
        at QueryInformationHandler,     dd        0
        at ReconfigureHandler,          dd        0
        at ResetHandler,                dd        0
        at SendHandler,                 dd        0
        at SetInformationHandler,       dd        0
        at TransferDataHandler,         dd        0
        at ReturnPacketHandler,                 dd        0
        at SendPacketsHandler,       dd        0
        at AllocateCompleteHandler,         dd        0
    iend



NdisDriverEntry:

    push 0; //fake length
    push DrvStruc
    push 0 ;handle
    call NdisMRegisterMiniport

    mov esi, eax
    cmp esi, 0
    jz nde_suc
        nop

    nde_suc:

ret

badalloc: db "Failed memory allocation", 0
goodalloc: db "Memory allocation succeeded", 0

NdisInitializeHandler:
    push 0
    push 0
    push 0
    push 0
    call NdisAllocateMemory
    cmp eax, 0
    jz ok1
    push badalloc
    call s2e_print_message
    add esp,4
    ret 4

ok1:
    push goodalloc
    call s2e_print_message
    add esp,4


ret 4

NdisQueryInformationHandler:
    xor eax, eax
ret 4


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
ndis_module: db "lan9000.sys", 0

imp_ndis_dll: db "ndis.sys",0
imp_NdisMRegisterMiniport: db "NdisMRegisterMiniport",0
imp_NdisAllocateMemory: db "NdisAllocateMemory",0

test_ndis:
    push NdisMRegisterMiniport
    push imp_NdisMRegisterMiniport
    push imp_ndis_dll
    call s2e_raw_load_import
    add esp, 3*4

    push NdisAllocateMemory
    push imp_NdisAllocateMemory
    push imp_ndis_dll
    call s2e_raw_load_import
    add esp, 3*4

    push NdisDriverEntry
    push 0xe0000
    push ndis_module
    call s2e_raw_load_module
    add esp, 3*4


    call NdisDriverEntry
    xor eax, eax

    call NdisInitializeHandler
    xor eax, eax

    call s2e_kill_state
lp1:

    cli
    hlt
    jmp lp1
    ret



