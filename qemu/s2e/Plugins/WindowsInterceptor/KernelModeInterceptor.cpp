#define NDEBUG

// XXX: qemu stuff should be included before anything from KLEE or LLVM !
extern "C" {
#include "config.h"
//#include "cpu.h"
//#include "exec-all.h"
#include "qemu-common.h"
}

#include "KernelModeInterceptor.h"
#include "WindowsImage.h"

#include <s2e/Utils.h>
#include <s2e/QemuKleeGlue.h>
#include <s2e/Interceptor/ExecutableImage.h>

#include <string>

using namespace s2e;
using namespace plugins;

WindowsKmInterceptor::WindowsKmInterceptor(WindowsMonitor *Os)
{
  m_Os = Os;
  
}


WindowsKmInterceptor::~WindowsKmInterceptor()
{

}

void WindowsKmInterceptor::NotifyDriverLoad(S2EExecutionState *State, ModuleDescriptor &Desc)
{
    WindowsImage Image(Desc.LoadBase);

    Desc.Pid = 0;
    Desc.NativeBase = Image.GetImageBase();
    Desc.Size = Image.GetImageSize();

    m_Os->onModuleLoad.emit(State, Desc);
}

void WindowsKmInterceptor::NotifyDriverUnload(S2EExecutionState *State, const ModuleDescriptor &Desc)
{
    m_Os->onModuleUnload.emit(State, Desc);
}

bool WindowsKmInterceptor::ReadModuleList(S2EExecutionState *state)
{
    uint32_t pListHead, pItem, pModuleEntry;
    uint32_t PsLoadedModuleList;
    s2e::windows::LIST_ENTRY32 ListHead;
    s2e::windows::MODULE_ENTRY32 ModuleEntry;
    uint32_t KdVersionBlock;

    if (!QEMU::ReadVirtualMemory(KD_VERSION_BLOCK, &KdVersionBlock, sizeof(KdVersionBlock))) {
        return false;
    }

    //PsLoadedModuleList = KdVersionBlock + PS_LOADED_MODULE_LIST_OFFSET;
    if (!QEMU::ReadVirtualMemory(KdVersionBlock + PS_LOADED_MODULE_LIST_OFFSET, &PsLoadedModuleList, sizeof(PsLoadedModuleList))) {
        return false;
    }

    pListHead = PsLoadedModuleList;
    if (!QEMU::ReadVirtualMemory(PsLoadedModuleList, &ListHead, sizeof(ListHead))) {
        return false;
    }

    
    for (pItem = ListHead.Flink; pItem != pListHead; ) {
        pModuleEntry = pItem;
        
        if (QEMU::ReadVirtualMemory(pModuleEntry, &ModuleEntry, sizeof(ModuleEntry)) < 0) {
            std::cout << "Could not load MODULE_ENTRY" << std::endl;
            return false;
        }

        DPRINTF("DRIVER_OBJECT Start=%#x Size=%#x DriverName=%s\n", Me.base, 
            0, ModuleName.c_str());

        ModuleDescriptor desc;
        desc.Pid = 0;
        desc.Name = QEMU::GetUnicode(ModuleEntry.driver_Name.Buffer, ModuleEntry.driver_Name.Length);
        desc.NativeBase = 0; // Image.GetImageBase();
        desc.LoadBase = ModuleEntry.driver_start; 

        NotifyDriverLoad(state, desc);

        pItem = ListHead.Flink;
        if (!QEMU::ReadVirtualMemory(ListHead.Flink, &ListHead, sizeof(ListHead))) {
            return false;
        }
    }

    return true;

}

bool WindowsKmInterceptor::GetDriverDescriptor(uint64_t pDriverObject, ModuleDescriptor &Desc)
{
    s2e::windows::DRIVER_OBJECT32 DrvObject;
    s2e::windows::MODULE_ENTRY32 Me;
    std::string ModuleName;

    if (!QEMU::ReadVirtualMemory(pDriverObject, 
        &DrvObject, sizeof(DrvObject))) {
            DPRINTF("Could not load DRIVER_OBJECT\n");
            return false;
    }

    DPRINTF("DRIVER_OBJECT Start=%#x Size=%#x\n", DrvObject.DriverStart, 
        DrvObject.DriverSize);

    if (DrvObject.DriverStart & 0xFFF) {
        std::cout << "Warning: The driver is not loaded on a page boundary" << std::endl;
    }


    //Fetch MODULE_ENTRY
    if (!DrvObject.DriverSection) {
        std::cout << "Null driver section" << std::endl;
        return false;
    }

    if (QEMU::ReadVirtualMemory(DrvObject.DriverSection, &Me, sizeof(Me)) < 0) {
        std::cout << "Could not load MODULE_ENTRY" << std::endl;
        return false;
    }

    ModuleName = QEMU::GetUnicode(Me.driver_Name.Buffer, Me.driver_Name.Length);
    DPRINTF("DRIVER_OBJECT Start=%#x Size=%#x DriverName=%s\n", Me.base, 
        0, ModuleName.c_str());

    Desc.Pid = 0;
    Desc.Name = ModuleName;
    Desc.NativeBase = 0; // Image.GetImageBase();
    Desc.LoadBase = DrvObject.DriverStart;
    Desc.Size = DrvObject.DriverSize;

    return true;
}

bool WindowsKmInterceptor::CatchModuleLoad(S2EExecutionState *state)
{
   CPUState *cpuState = (CPUState *)state->getCpuState();
    assert(m_Os->GetVersion() == WindowsMonitor::SP3);

    uint64_t pDriverObject;

    if (!QEMU::ReadVirtualMemory(cpuState->regs[R_ESP], &pDriverObject, m_Os->GetPointerSize())) {
        return false;
    }

    if (!pDriverObject) {
        std::cout << "DriverObject is NULL" << std::endl;
        return false;
    }

    ModuleDescriptor desc;
    if (!GetDriverDescriptor(pDriverObject, desc)) {
        return false;
    }

    NotifyDriverLoad(state, desc);
    return true;
}

bool WindowsKmInterceptor::CatchModuleUnload(S2EExecutionState *state)
{
    CPUState *cpuState = (CPUState *)state->getCpuState();

    uint64_t pDriverObject;

    if (!QEMU::ReadVirtualMemory(cpuState->regs[R_ESP] + 4, &pDriverObject, m_Os->GetPointerSize())) {
        return false;
    }

    if (!pDriverObject) {
        std::cout << "DriverObject is NULL" << std::endl;
        return false;
    }

    ModuleDescriptor desc;
    if (!GetDriverDescriptor(pDriverObject, desc)) {
        return false;
    }

    NotifyDriverUnload(state, desc);
    
    return true;
}
