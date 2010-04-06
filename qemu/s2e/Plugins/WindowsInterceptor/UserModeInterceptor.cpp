#include "UserModeInterceptor.h"
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

WindowsUmInterceptor::WindowsUmInterceptor(WindowsMonitor *Os)
{
  
  m_Os = Os;
  m_TracingState = SEARCH_PROCESS;
  m_PrevCr3 = 0;

  m_ASBase = 0;
  m_ASSize = Os->GetUserAddressSpaceSize();
}


WindowsUmInterceptor::~WindowsUmInterceptor()
{

}

int WindowsUmInterceptor::FindModules(void *CpuState)
{
  s2e::windows::LDR_DATA_TABLE_ENTRY32 LdrEntry;
  s2e::windows::PEB_LDR_DATA32 LdrData;
  CPUState *State = (CPUState*)CpuState;
  int Result = 0;

  if (!WaitForProcessInit(CpuState)) {
    return -1;
  }

  if (QEMU::ReadVirtualMemory(m_LdrAddr, &LdrData, sizeof(s2e::windows::PEB_LDR_DATA32)) < 0) {
    return -1;
  }

  uint32_t CurLib = CONTAINING_RECORD32(LdrData.InLoadOrderModuleList.Flink, 
    s2e::windows::LDR_DATA_TABLE_ENTRY32, InLoadOrderLinks);

  uint32_t HeadOffset = m_LdrAddr + offsetof(s2e::windows::PEB_LDR_DATA32, InLoadOrderModuleList);
  if (LdrData.InLoadOrderModuleList.Flink == HeadOffset)
    return -1;

  do {
    if (QEMU::ReadVirtualMemory(CurLib, &LdrEntry, sizeof(s2e::windows::LDR_DATA_TABLE_ENTRY32)) < 0 ) {
      DPRINTF("Could not read LDR_DATA_TABLE_ENTRY (%#x)\n", CurLib);
      return Result;
    }

    std::string s = QEMU::GetUnicode(LdrEntry.BaseDllName.Buffer, LdrEntry.BaseDllName.Length);

    //if (m_SearchedModules.find(s) != m_SearchedModules.end()) {
      //Update the information about the library
      ModuleDescriptor Desc; 
      Desc.Pid = State->cr[3];
      Desc.Name = s;
      Desc.LoadBase = LdrEntry.DllBase;
      Desc.Size = LdrEntry.SizeOfImage;
      
      Result = 1;
      
      if (m_LoadedLibraries.find(Desc) == m_LoadedLibraries.end()) {
        DPRINTF("  MODULE %s Base=%#x Size=%#x\n", s.c_str(), LdrEntry.DllBase, LdrEntry.SizeOfImage);
        m_LoadedLibraries.insert(Desc);
        NotifyLibraryLoad(Desc);
      }
      
    CurLib = CONTAINING_RECORD32(LdrEntry.InLoadOrderLinks.Flink, 
      s2e::windows::LDR_DATA_TABLE_ENTRY32, InLoadOrderLinks);
  }while(LdrEntry.InLoadOrderLinks.Flink != HeadOffset);

  return Result;
}


bool WindowsUmInterceptor::WaitForProcessInit(void *CpuState)
{
  CPUState *state = (CPUState *)CpuState;
  s2e::windows::PEB_LDR_DATA32 LdrData;
  s2e::windows::PEB32 PebBlock;
  uint32_t Peb = (uint32_t)-1;
  

  if (QEMU::ReadVirtualMemory(state->segs[R_FS].base + 0x18, &Peb, 4) < 0) {
    return false;
  }
  
  if (QEMU::ReadVirtualMemory(Peb+0x30, &Peb, 4) < 0) {
    return false;
  }

  if (Peb != 0xFFFFFFFF) {
//      DPRINTF("peb=%x eip=%x cr3=%x\n", Peb, state->eip, state->cr[3] );
    }
    else
      return false;
  
  if (!QEMU::ReadVirtualMemory(Peb, &PebBlock, sizeof(PebBlock))) {
      return false;
  }

  /* Check that the entries are inited */
  if (!QEMU::ReadVirtualMemory(PebBlock.Ldr, &LdrData, 
    sizeof(s2e::windows::PEB_LDR_DATA32))) {
    return false;
  }

  /* Check that the structure is correctly initialized */
  if (LdrData.Length != 0x28) 
    return false;
  
  if (!LdrData.InLoadOrderModuleList.Flink || !LdrData.InLoadOrderModuleList.Blink ) 
    return false;
  
  if (!LdrData.InMemoryOrderModuleList.Flink || !LdrData.InMemoryOrderModuleList.Blink ) 
    return false;
  
  m_LdrAddr = PebBlock.Ldr;
  m_ProcBase = PebBlock.ImageBaseAddress;

  DPRINTF("Process %#"PRIx64" %#x %#x\n", m_ProcBase, LdrData.Initialized, LdrData.EntryInProgress);
  return true;

}


void WindowsUmInterceptor::NotifyProcessLoad()
{
#if 0
  WindowsImage Image(m_ProcBase);

  ModuleDescriptor Desc;
  Desc.Name = m_ProcessName;
  Desc.NativeBase = Image.GetImageBase();
  Desc.LoadBase = m_ProcBase;
  Desc.Size = Image.GetImageSize();

  const IExecutableImage::Imports &I = Image.GetImports();
  const IExecutableImage::Exports &E = Image.GetExports();

  m_Os->onProcessLoad.emit(Desc, I, E);
#endif
}

void WindowsUmInterceptor::NotifyLibraryLoad(const ModuleDescriptor &Library)
{

  ModuleDescriptor MD = Library;
  
  WindowsImage Image(MD.LoadBase);
  MD.NativeBase = Image.GetImageBase();
  const IExecutableImage::Imports &I = Image.GetImports();
  const IExecutableImage::Exports &E = Image.GetExports();

  m_Os->onModuleLoad.emit(MD, I, E);

}

bool WindowsUmInterceptor::CatchModuleLoad(void *CpuState)
{
  FindModules(CpuState);
  return true;
}

bool WindowsUmInterceptor::CatchProcessTermination(void *CpuState)
{
   CPUState *state = (CPUState *)CpuState;

   uint64_t pEProcess = state->regs[R_EBX];
   s2e::windows::EPROCESS32 EProcess;

   if (!QEMU::ReadVirtualMemory(pEProcess, &EProcess, sizeof(EProcess))) {
      return false;
   }

   DPRINTF("Process %#"PRIx32" %16s unloaded\n", EProcess.Pcb.DirectoryTableBase,
      EProcess.ImageFileName);
   m_Os->onProcessUnload.emit(EProcess.Pcb.DirectoryTableBase);
    
   return true;  
}

bool WindowsUmInterceptor::CatchModuleUnload(void *CpuState)
{
   CPUState *state = (CPUState *)CpuState;

   uint64_t pLdrEntry = state->regs[R_EBX];
   s2e::windows::LDR_DATA_TABLE_ENTRY32 LdrEntry;

   if (!QEMU::ReadVirtualMemory(pLdrEntry, &LdrEntry, sizeof(LdrEntry))) {
      return false;
   }

  
   ModuleDescriptor Desc; 
   Desc.Pid = state->cr[3];
   Desc.Name = QEMU::GetUnicode(LdrEntry.BaseDllName.Buffer, LdrEntry.BaseDllName.Length);;
   Desc.LoadBase = LdrEntry.DllBase;
   Desc.Size = LdrEntry.SizeOfImage;

   DPRINTF("Detected module unload %s pid=%#"PRIx64" LoadBase=%#"PRIx64"\n",
      Desc.Name.c_str(), Desc.Pid, Desc.LoadBase);

   return true;
}