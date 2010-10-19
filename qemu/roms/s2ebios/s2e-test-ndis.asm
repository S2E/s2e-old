
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
stall: db "Stalling processor", 0

KeStallExecutionProcessor:
    push stall
    call s2e_print_message
    add esp,4
ret 4

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
rtleq: db "Executing RtlEqualUnicodeString", 0
RtlEqualUnicodeString:
    push rtleq
    call s2e_print_message
    add esp,4
    xor eax, eax
ret 12

uptime: db "Executing GetSystemUpTime", 0
GetSystemUpTime:
    push uptime
    call s2e_print_message
    add esp,4
    xor eax, eax
ret 4


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
NdisMRegisterMiniport:
mov eax, 0
ret 12

NdisAllocateMemory:
mov eax, 0
ret 0x10

nmqar: db "Executing NdisMQueryAdapterResources", 0
NdisMQueryAdapterResources:
    push nmqar
    call s2e_print_message
    add esp,4
    
    mov eax, [esp + 4]
    mov dword [eax], 0 ; Success
    
    xor eax, eax
ret 4*4

allocshared: db "Executing NdisMAllocateSharedMemory", 0
NdisMAllocateSharedMemory:
    push allocshared
    call s2e_print_message
    add esp,4

    mov eax, [esp + 4*(1+3)]  ;Store the virtual address of the mapping
    mov dword [eax], 0x90000

    mov eax, [esp + 4*(1+4)]  ;Return the physical address to be used for DMA
    mov dword [eax], 0x90000
    mov dword [eax+4], 0x0000

    xor eax, eax
    ret 4*(5)

freeshared: db "Executing NdisMFreeSharedMemory", 0
NdisMFreeSharedMemory:
   push freeshared
   call s2e_print_message
   add esp,4

   ret 4*(5+1)

mapiospace: db "Executing NdisMMapIoSpace", 0
NdisMMapIoSpace:
    push allocshared
    call s2e_print_message
    add esp,4

    xor eax, eax
ret 4*4


slotinfo: db "Executing NdisReadPciSlotInformation", 0
NdisReadPciSlotInformation:
    push slotinfo
    call s2e_print_message
    add esp,4
    xor eax, eax
ret 4*5


inittimermsg: db "Executing NdisMInitializeTimer", 0
NdisMInitializeTimer:
    push inittimermsg
    call s2e_print_message
    add esp,4
    xor eax, eax
ret 4*4

settimermsg: db "Executing NdisSetTimer", 0
NdisSetTimer:
    push settimermsg
    call s2e_print_message
    add esp,4
    xor eax, eax
ret 4*4




rdnetaddr: db "Executing NdisReadNetworkAddress", 0

NdisReadNetworkAddress:
push ebp
mov ebp, esp

mov eax, [ebp + 0x8] ;Status
mov dword [eax], 0

mov eax, [ebp + 0xC] ;Network address
mov dword [eax], 0x90000 ;This must be outside the bios, in ram

mov eax, [ebp + 0x10] ;Network address size
mov dword [eax], 16

    push rdnetaddr
    call s2e_print_message
    add esp,4
    xor eax, eax

pop ebp
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
        at QueryInformationHandler,     dd        NdisQueryInformationHandler
        at ReconfigureHandler,          dd        0
        at ResetHandler,                dd        0
        at SendHandler,                 dd        NdisSendHandler
        at SetInformationHandler,       dd        0
        at TransferDataHandler,         dd        0
        at ReturnPacketHandler,         dd        0
        at SendPacketsHandler,          dd        0
        at AllocateCompleteHandler,     dd        0
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

ret 2*4

badalloc: db "Failed memory allocation", 0
goodalloc: db "Memory allocation succeeded", 0

NdisInitializeHandler:
    push 0
    push 0
    push 0
    push 0
    call NdisAllocateMemory
    mov esi, eax
    cmp eax, 0
    jz ok1
    push badalloc
    call s2e_print_message
    add esp,4

    mov eax, esi
    ret 4

ok1:
    push goodalloc
    call s2e_print_message
    add esp,4

    mov eax, esi


ret 4

queryinfomsg: db "Calling QueryInformationHandler", 0
NdisQueryInformationHandler:
    push ebp
    mov ebp, esp

    push queryinfomsg
    call s2e_print_message
    add esp,4

    mov eax, [ebp + 4 + 2*4]
    cmp eax, 3
    jz nqih3
    mov eax, 1
    jmp nqihe

nqih3:
    xor eax, eax

nqihe:
    pop ebp
    ret 6*4


NdisSendHandler:

ret 3*4

timerhandler1_msg: db "Running timer handler 1", 0
NdisTimerHandler1:
    push timerhandler1_msg
    call s2e_print_message
    add esp,4

    mov eax, [esp + 8]
    push 0
    push eax
    call s2e_print_expression
    add esp, 8
ret 4*4

timerhandler2_msg: db "Running timer handler 2", 0
NdisTimerHandler2:
    push timerhandler2_msg
    call s2e_print_message
    add esp,4

    mov eax, [esp + 8]
    push 0
    push eax
    call s2e_print_expression
    add esp, 8
ret 4*4

timerhandler3_msg: db "Running timer handler 3", 0
NdisTimerHandler3:
    push timerhandler3_msg
    call s2e_print_message
    add esp,4

    mov eax, [esp + 8]
    push 0
    push eax
    call s2e_print_expression
    add esp, 8
ret 4*4

timerhandler4_msg: db "Running timer handler 4", 0
NdisTimerHandler4:
    push timerhandler4_msg
    call s2e_print_message
    add esp,4

    mov eax, [esp + 8]
    push 0
    push eax
    call s2e_print_expression
    add esp, 8
ret 4*4

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
ndis_module: db "lan9000.sys", 0

imp_ndis_dll: db "ndis.sys",0
imp_NdisMRegisterMiniport: db "NdisMRegisterMiniport",0
imp_NdisMQueryAdapterResources: db "NdisMQueryAdapterResources",0
imp_NdisMAllocateSharedMemory: db "NdisMAllocateSharedMemory",0
imp_NdisMFreeSharedMemory: db "NdisMFreeSharedMemory", 0
imp_NdisMMapIoSpace: db "NdisMMapIoSpace",0
imp_NdisMInitializeTimer: db "NdisMInitializeTimer",0
imp_NdisSetTimer: db "NdisSetTimer",0
imp_NdisAllocateMemory: db "NdisAllocateMemory",0
imp_NdisReadNetworkAddress: db "NdisReadNetworkAddress",0
imp_NdisReadPciSlotInformation: db "NdisReadPciSlotInformation",0

imp_ntoskrnl_exe: db "ntoskrnl.exe",0
imp_GetSystemUpTime: db "GetSystemUpTime", 0
imp_RtlEqualUnicodeString: db "RtlEqualUnicodeString", 0

imp_hal_dll: db "hal.dll",0
imp_KeStallExecutionProcessor: db "KeStallExecutionProcessor", 0

tmp1: dd 0

test_ndis:
    push ebp
    mov ebp, esp
    sub esp, 0x20

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

    push NdisReadNetworkAddress
    push imp_NdisReadNetworkAddress
    push imp_ndis_dll
    call s2e_raw_load_import
    add esp, 3*4

    push NdisReadPciSlotInformation
    push imp_NdisReadPciSlotInformation
    push imp_ndis_dll
    call s2e_raw_load_import
    add esp, 3*4

    push NdisMQueryAdapterResources
    push imp_NdisMQueryAdapterResources
    push imp_ndis_dll
    call s2e_raw_load_import
    add esp, 3*4

    push NdisMAllocateSharedMemory
    push imp_NdisMAllocateSharedMemory
    push imp_ndis_dll
    call s2e_raw_load_import
    add esp, 3*4

    push NdisMFreeSharedMemory
    push imp_NdisMFreeSharedMemory
    push imp_ndis_dll      
    call s2e_raw_load_import
    add esp, 3*4

    push NdisMMapIoSpace
    push imp_NdisMMapIoSpace
    push imp_ndis_dll
    call s2e_raw_load_import
    add esp, 3*4

    push NdisMInitializeTimer
    push imp_NdisMInitializeTimer
    push imp_ndis_dll
    call s2e_raw_load_import
    add esp, 3*4

    push NdisSetTimer
    push imp_NdisSetTimer
    push imp_ndis_dll
    call s2e_raw_load_import
    add esp, 3*4

    ;---------------------------
    push KeStallExecutionProcessor
    push imp_KeStallExecutionProcessor
    push imp_hal_dll
    call s2e_raw_load_import
    add esp, 3*4

    ;---------------------------
    push GetSystemUpTime
    push imp_GetSystemUpTime
    push imp_ntoskrnl_exe
    call s2e_raw_load_import
    add esp, 3*4

    push RtlEqualUnicodeString
    push imp_RtlEqualUnicodeString
    push imp_ntoskrnl_exe
    call s2e_raw_load_import
    add esp, 3*4
    ;---------------------------

    push NdisDriverEntry
    push 0xe0000
    push ndis_module
    call s2e_raw_load_module
    add esp, 3*4

    push 0
    push 0
    call NdisDriverEntry
    xor eax, eax

    ;-----------------
    ;Modify this to call different tests
    call ndis_test_symbmmio
    jmp lp1
    ;-----------------


    push 0
    push 0
    push 0
    push 0
    push 0
    push 0
    call NdisInitializeHandler
    xor eax, eax

    push 0
    push 0
    push 0
    call NdisSendHandler

    push 1234
    call KeStallExecutionProcessor

    call GetSystemUpTime

    push 1
    push 3
    push 4
    call RtlEqualUnicodeString
    
    push 12; Length
    push 0x90000; Buffer
    push 0 ;Offset
    push 0 ;SlotNumber
    push 0 ;Handle
    call NdisReadPciSlotInformation
    
    cmp eax, 0
    jnz nrpsi
    call s2e_kill_state
nrpsi:

    push 0x90000; BufferSize
    push 0x90100; ResourceList
    push 0 ;context
    push 0x90004; status
    call NdisMQueryAdapterResources
    
    cmp dword[0x90004], 0
    jnz nmqar1
    call s2e_kill_state

nmqar1:

    push 0x90004; PhysicalAddress
    push 0x90000; VirtualAddress
    push 0 ;cached
    push 0x1000; length
    push 0x000; handle
    call NdisMAllocateSharedMemory

    cmp dword[0x90000], 0
    jnz nmasm
    call s2e_kill_state

nmasm:


    push 0x90004; PhysicalAddress
    push 0x90000; VirtualAddress
    push 0 ;cached
    push 0x1000; length
    push 0x000; handle
    call NdisMMapIoSpace

    cmp eax, 0
    jz nmis
    call s2e_kill_state

nmis:

    ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
    push 0
    lea eax, [ebp - 4]
    push eax;pLength

    lea eax, [ebp - 8]
    push eax ;pAddress

    lea eax, [ebp - 0xc]
    push eax;pStatus
    call NdisReadNetworkAddress

    cmp dword [ebp - 0xc], 0
    jz nrna_suc
    call s2e_kill_state

nrna_suc:
    push 1
    push 2
    push 3
    push 4
    push 3
    push 5
    call NdisQueryInformationHandler

    mov eax, [esp - 5*4]
    mov [0x90000], eax

    push 0
    push 4
    push 0x90000
    call s2e_print_memory
    add esp, 3*4

    ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

    push 0x1111
    push NdisTimerHandler1
    push 0
    push 0
    call NdisMInitializeTimer

    push 0x2222
    push NdisTimerHandler2
    push 0
    push 0
    call NdisMInitializeTimer

    push 0x3333
    push NdisTimerHandler3
    push 0
    push 0
    call NdisMInitializeTimer

    push 0x4444
    push NdisTimerHandler4
    push 0
    push 0
    call NdisMInitializeTimer

    ;Calling one of the handlers
    push 0
    push 0
    push 0x1111
    push 0
    call NdisTimerHandler1

    ;Calling one of the handlers
    push 0
    push 0
    push 0x1111
    push 0
    call NdisTimerHandler1

    ;Calling one of the handlers
    push 0
    push 0
    push 0x1111
    push 0
    call NdisTimerHandler1

    ;Calling one of the handlers
    push 0
    push 0
    push 0x1111
    push 0
    call NdisTimerHandler1



    push 0
    push 0
    call s2e_kill_state
    add esp, 8
lp1:


    leave

    cli
    hlt
    jmp lp1
    ret



nts_err1: db "Incorrect value read from DMA area", 0
nts_err2: db "Invalid value read from normal memory", 0
nts_err3: db "NdisMAllocateSharedMemory failed", 0
nts_okmsg: db "All is fine", 0

ndis_test_symbmmio:
    %push mycontext
    %stacksize flat
    %assign %$localsize 0
    %local virtual_address:dword, physical_address:qword, length:dword


    enter %$localsize,0

    mov dword [length], 0x1234

    lea eax, [physical_address] ;PhysicalAddress
    push eax
    lea eax, [virtual_address] ;VirtualAddress
    push eax
    push 0 ;cached
    push dword [length]; length
    push 0x000; handle
    call NdisMAllocateSharedMemory

    cmp dword [virtual_address], 0
    jnz nts_ok

    push nts_err3
    push 0
    call s2e_kill_state
    add esp, 8

nts_ok:

    ;Check that reading/writing past the zone works
    mov eax, 0xBADCAFE
    mov edx, [virtual_address]
    add edx, [length]
    mov [edx], eax
    mov ecx, [edx]
    cmp [edx], eax
    je nts_ok11

    ;Kill state because of incorrect read value
    push nts_err1
    push ecx
    call s2e_kill_state
    add esp, 8

nts_ok11:

    ;Try to access the memory
    mov eax, [virtual_address]
    mov ecx, [length]
    shr ecx, 1
    add eax, ecx
    cmp dword [eax], 0
    je nts_ok12

    nop

nts_ok12:

    ;Free the allocated zone
    push dword [physical_address+4]; PhysicalAddress
    push dword [physical_address]; PhysicalAddress
    push dword [virtual_address]; VirtualAddress
    push 0 ;cached
    push dword [length]; length
    push 0x000; handle
    call NdisMFreeSharedMemory

    ;Check that a read does not return symbolic values anymore
    mov edx, [virtual_address]
    add edx, 0x4

    mov eax, 0xBADCAFE
    mov [edx], eax
    mov ecx, [edx]
    cmp [edx], eax
    je nts_ok1

    ;Print an error message
    push nts_err2
    push ecx
    call s2e_kill_state
    add esp, 8
    ret

nts_ok1:
    push nts_okmsg
    push 0
    call s2e_kill_state
    add esp, 8
    leave
    ret

    %pop
