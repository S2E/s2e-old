#include "KernelModeInterceptor.h"
#include "WindowsImage.h"

#include <s2e/Utils.h>
#include <s2e/QemuKleeGlue.h>
#include <s2e/Interceptor/ExecutableImage.h>

#include <string>

extern "C" {
#include "config.h"
#include "cpu.h"
#include "exec-all.h"
#include "qemu-common.h"
}


using namespace s2e;
using namespace plugins;

WindowsKmInterceptor::WindowsKmInterceptor(WindowsMonitor *Os)
{
  m_Os = Os;
  
}


WindowsKmInterceptor::~WindowsKmInterceptor()
{

}

void WindowsKmInterceptor::NotifyDriverLoad(ModuleDescriptor &Desc)
{
    WindowsImage Image(Desc.LoadBase);

    Desc.Pid = 0;
    Desc.NativeBase = Image.GetImageBase();

    const IExecutableImage::Imports &I = Image.GetImports();
    const IExecutableImage::Exports &E = Image.GetExports();

    m_Os->onModuleLoad.emit(Desc, I, E);
}

void WindowsKmInterceptor::NotifyDriverUnload(const ModuleDescriptor &Desc)
{
    m_Os->onModuleUnload.emit(Desc);
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

bool WindowsKmInterceptor::CatchModuleLoad(void *CpuState)
{
    CPUState *state = (CPUState *)CpuState;
    assert(m_Os->GetVersion() == WindowsMonitor::SP3);

    uint64_t pDriverObject;

    if (!QEMU::ReadVirtualMemory(state->regs[R_ESP], &pDriverObject, m_Os->GetPointerSize())) {
        return false;
    }

    if (!pDriverObject) {
        std::cout << "DriverObject is NULL" << std::endl;
        return false;
    }

    ModuleDescriptor Desc;
    if (!GetDriverDescriptor(pDriverObject, Desc)) {
        return false;
    }

    NotifyDriverLoad(Desc);
    return true;
}

bool WindowsKmInterceptor::CatchModuleUnload(void *CpuState)
{
    CPUState *state = (CPUState *)CpuState;

    uint64_t pDriverObject;

    if (!QEMU::ReadVirtualMemory(state->regs[R_ESP] + 4, &pDriverObject, m_Os->GetPointerSize())) {
        return false;
    }

    if (!pDriverObject) {
        std::cout << "DriverObject is NULL" << std::endl;
        return false;
    }

    ModuleDescriptor Desc;
    if (!GetDriverDescriptor(pDriverObject, Desc)) {
        return false;
    }

    NotifyDriverUnload(Desc);
    
    return true;
}