#include "UserModeInterceptor.h"
#include "WindowsImage.h"

#include <s2e/Utils.h>

#include <string>

extern "C" {
#include "config.h"
#include "cpu.h"
#include "exec-all.h"
#include "qemu-common.h"
}

using namespace std;

CUserModeInterceptor::CUserModeInterceptor(CWindowsOS *Os):IInterceptor()
{
  
  m_Os = Os;
  m_TracingState = SEARCH_PROCESS;
  m_PrevCr3 = 0;

  m_ASBase = 0;
  m_ASSize = Os->GetUserAddressSpaceSize();
  m_Events = NULL;
}


CUserModeInterceptor::~CUserModeInterceptor()
{

}

int CUserModeInterceptor::FindModules()
{
  reveng_windows::LDR_DATA_TABLE_ENTRY32 LdrEntry;
  reveng_windows::PEB_LDR_DATA32 LdrData;
  int Result = 0;

  if (s_Api.ReadVirtualMemory(m_LdrAddr, &LdrData, sizeof(reveng_windows::PEB_LDR_DATA32)) < 0) {
    return -1;
  }

  uint32_t CurLib = CONTAINING_RECORD32(LdrData.InLoadOrderModuleList.Flink, 
    reveng_windows::LDR_DATA_TABLE_ENTRY32, InLoadOrderLinks);

  uint32_t HeadOffset = m_LdrAddr + offsetof(reveng_windows::PEB_LDR_DATA32, InLoadOrderModuleList);
  if (LdrData.InLoadOrderModuleList.Flink == HeadOffset)
    return -1;

  do {
    if (s_Api.ReadVirtualMemory(CurLib, &LdrEntry, sizeof(reveng_windows::LDR_DATA_TABLE_ENTRY32)) < 0 ) {
      DPRINTF("Could not read LDR_DATA_TABLE_ENTRY (%#x)\n", CurLib);
      return Result;
    }

    string s = s_Api.GetUnicode(LdrEntry.BaseDllName.Buffer, LdrEntry.BaseDllName.Length);

    //if (m_SearchedModules.find(s) != m_SearchedModules.end()) {
      DPRINTF("  Dll %s Base=%#x Size=%#x\n", s.c_str(), LdrEntry.DllBase, LdrEntry.SizeOfImage);
      
      //Update the information about the library
      ModuleDescriptor Desc; 
      Desc.Name = s;
      Desc.LoadBase = LdrEntry.DllBase;
      Desc.Size = LdrEntry.SizeOfImage;
      
      Result = 1;
      
      if (m_LoadedLibraries.find(Desc) == m_LoadedLibraries.end()) {
        m_LoadedLibraries.insert(Desc);
        NotifyLibraryLoad(Desc);
      }
      
    CurLib = CONTAINING_RECORD32(LdrEntry.InLoadOrderLinks.Flink, 
      reveng_windows::LDR_DATA_TABLE_ENTRY32, InLoadOrderLinks);
  }while(LdrEntry.InLoadOrderLinks.Flink != HeadOffset);

  return Result;
}


bool CUserModeInterceptor::WaitForProcessInit(void *CpuState)
{
  CPUState *state = (CPUState *)CpuState;
  reveng_windows::PEB_LDR_DATA32 LdrData;
  reveng_windows::PEB32 PebBlock;
  uint32_t Peb = (uint32_t)-1;
  

  if (s_Api.ReadVirtualMemory(state->segs[R_FS].base + 0x18, &Peb, 4) < 0) {
    return false;
  }
  
  if (s_Api.ReadVirtualMemory(Peb+0x30, &Peb, 4) < 0) {
    return false;
  }

  if (Peb != 0xFFFFFFFF) {
//      DPRINTF("peb=%x eip=%x cr3=%x\n", Peb, state->eip, state->cr[3] );
    }
    else
      return false;
  
  if (!s_Api.ReadVirtualMemory(Peb, &PebBlock, sizeof(PebBlock))) {
      return false;
  }

  /* Check that the entries are inited */
  if (!s_Api.ReadVirtualMemory(PebBlock.Ldr, &LdrData, 
    sizeof(reveng_windows::PEB_LDR_DATA32))) {
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

  DPRINTF("Process %#I64x %#x %#x\n", m_ProcBase, LdrData.Initialized, LdrData.EntryInProgress);
  return true;

}


void CUserModeInterceptor::NotifyProcessLoad()
{
  if (!m_Events) {
    return;
  }

  WindowsImage Image(m_ProcBase);

  ModuleDescriptor Desc;
  Desc.Name = m_ProcessName;
  Desc.NativeBase = Image.GetImageBase();
  Desc.LoadBase = m_ProcBase;
  Desc.Size = Image.GetImageSize();

  const IExecutableImage::Imports &I = Image.GetImports();
  const IExecutableImage::Exports &E = Image.GetExports();

  m_Events->OnProcessLoad(this, Desc, I, E);
}

void CUserModeInterceptor::NotifyLibraryLoad(const ModuleDescriptor &Library)
{
  if (!m_Events) {
    return;
  }

  ModuleDescriptor MD = Library;
  
  WindowsImage Image(MD.LoadBase);
  MD.NativeBase = Image.GetImageBase();
  const IExecutableImage::Imports &I = Image.GetImports();
  const IExecutableImage::Exports &E = Image.GetExports();

  m_Events->OnLibraryLoad(this, MD, I, E);
}

bool CUserModeInterceptor::OnTbEnter(void *CpuState, bool Translation)
{
  CPUState *state = (CPUState *)CpuState;

  /******************************************/
  /* Update the list of libraries as they get loaded */
again:
  if (m_TracingState == WAIT_LIBRARY_LOAD && m_HookedCr3 == state->cr[3]) {
    if (state->eip != m_Os->GetLdrpCallInitRoutine()) {
      return false;
    }
    FindModules();
    return false;
  }

  /******************************************/
  if (state->cr[3] == m_PrevCr3 && m_TracingState != WAIT_PROCESS_INIT
    && m_TracingState != WAIT_LIBRARY_LOAD) 
  {
    return false;
  }
  
  m_PrevCr3 = state->cr[3];
  
  /******************************************/
  if (m_TracingState == SEARCH_PROCESS) {
    WindowsSpy &Spy = *m_Os->GetSpy();
    SProcessDescriptor PDesc;
    if (Spy.FindProcess(state->cr[3], m_ProcList, PDesc)) {
      /* Already seen that process, don't care about it */
      return false;
    }
    Spy.ScanProcesses(m_ProcList);
    if (Spy.FindProcess(m_ProcessName, m_ProcList, PDesc)) {
      m_TracingState = WAIT_PROCESS_INIT;
      m_HookedCr3 = PDesc.PageDirectory;
    }
  }
  
  /******************************************/
  if (m_TracingState == WAIT_PROCESS_INIT && state->cr[3] == m_HookedCr3) {
    if (!WaitForProcessInit(state)) {
      return false;
    }
    /* It is loaded! */
    NotifyProcessLoad();
    m_TracingState = WAIT_LIBRARY_LOAD;
    goto again;
  }

  return false;
}

bool CUserModeInterceptor::OnTbExit(void *CpuState, bool Translation)
{
  return false;
}


/*bool CUserModeInterceptor::DecideSymbExec(uint64_t cr3, uint64_t Pc)
{
  return false;
}*/

void CUserModeInterceptor::DumpInfo(std::ostream &os)
{
  
}
bool CUserModeInterceptor::GetModule(ModuleDescriptor &Desc)
{
  return false;
}


bool CUserModeInterceptor::SetModule(const std::string &Name)
{
  m_ProcessName = Name;
  return true;
}

void CUserModeInterceptor::SetEventHandler(struct IInterceptorEvent *Hdlr)
{
  m_Events = Hdlr;
}