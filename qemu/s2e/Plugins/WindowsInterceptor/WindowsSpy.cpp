#include <s2e/Utils.h>
#include "WindowsOS.h"
#include "WindowsSpy.h"
#include "WindowsImage.h"


#include <s2e/QemuKleeGlue.h>

extern "C" {
#include "config.h"
#include "cpu.h"
#include "exec-all.h"
#include "qemu-common.h"

}

using namespace std;
using namespace reveng_windows;

WindowsSpy::WindowsSpy(CWindowsOS *OS)
{
  m_OS = OS;
}

WindowsSpy::~WindowsSpy()
{

}


bool WindowsSpy::AddProcess(const SProcessDescriptor &ProcessOrig,
                            IDataStructureSpy::Processes &P)
{
  if (P.ByDir.find(ProcessOrig) != P.ByDir.end()) {
    return false;
  }
  
  P.ByName.insert(ProcessOrig);
  P.ByDir.insert(ProcessOrig);
  
  return true;
}

void WindowsSpy::ClearProcesses(IDataStructureSpy::Processes &P)
{
  P.ByName.clear();
  P.ByDir.clear();
}


/**
 *  Each system update might change the kernel build number.
 *  XXX: This should be fixed for each update, ideally put into 
 *  a configuration file.
 */
WindowsSpy::EWindowsVersion WindowsSpy::GetVersion()
{
  reveng_windows::KPRCB32 Kprcb;

  if (!QEMU::ReadVirtualMemory(KPRCB_OFFSET, &Kprcb, sizeof(KPRCB32)) ) {
    DPRINTF("Could not read KPRCB (%#x)\n", KPRCB_OFFSET);
    return UNKNOWN;
  }

  DPRINTF("Running Windows version %d.%d\n", Kprcb.MajorVersion, Kprcb.MinorVersion);

  return UNKNOWN;
}

bool WindowsSpy::ScanProcesses(IDataStructureSpy::Processes &P)
{
  KPRCB32 Kprcb;
  KTHREAD32 KThread;
  EPROCESS32 EProcess;

  if (!QEMU::ReadVirtualMemory(KPRCB_OFFSET, &Kprcb, sizeof(KPRCB32)) ) {
    DPRINTF("Could not read KPRCB (%#x)\n", KPRCB_OFFSET);
    return false;
  }

  if (!QEMU::ReadVirtualMemory(Kprcb.CurrentThread, &KThread, sizeof(KTHREAD32)) )  {
    DPRINTF("Could not read ETHREAD in KPRCB (%#x)\n", Kprcb.CurrentThread);
    return false;
  }

  /* Cycle through the process list and pick the specified one */
  uint32_t CurProcess = KThread.ApcState.Process;
  uint32_t LeHeadPtr = KThread.ApcState.Process + offsetof(EPROCESS32, ActiveProcessLinks);
  do {
    if (!QEMU::ReadVirtualMemory(CurProcess, &EProcess, sizeof(EPROCESS32))  ) {
      DPRINTF("Could not read EPROCESS in KThread.ApcState.Process (%#x)\n", CurProcess);
      return false;
    }
    if (!EProcess.ActiveProcessLinks.Flink) {
      /* Happens sometimes */
      return false;
    }

    DPRINTF("Process %s (DirTable=%#x)\n", EProcess.ImageFileName, EProcess.Pcb.DirectoryTableBase);

    SProcessDescriptor PDesc;
    PDesc.Name = string((const char*)EProcess.ImageFileName);
    PDesc.PageDirectory = EProcess.Pcb.DirectoryTableBase;
    AddProcess(PDesc, P);
    
    CurProcess = CONTAINING_RECORD32(EProcess.ActiveProcessLinks.Flink, EPROCESS32, ActiveProcessLinks);
  
  }while(EProcess.ActiveProcessLinks.Flink != LeHeadPtr);
  
  return true;
}

bool WindowsSpy::FindProcess(uint64_t cr3,
                             const IDataStructureSpy::Processes &P,
                             SProcessDescriptor &Result)
{
  SProcessDescriptor D;
  D.PageDirectory = cr3;
  
  IDataStructureSpy::ProcessesByDir::iterator it;
  it = P.ByDir.find(D);
  if (it == P.ByDir.end()) {
    return false;
  }

  Result = *it;
  return true;
}

bool WindowsSpy::FindProcess(const std::string &Name,
                             const IDataStructureSpy::Processes &P,
                             SProcessDescriptor &Result)
{
  SProcessDescriptor D;
  D.Name = Name;
  
  IDataStructureSpy::ProcessesByName::iterator it;
  it = P.ByName.find(D);
  if (it == P.ByName.end()) {
    return false;
  }

  Result = *it;
  return true;
}


bool WindowsSpy::GetCurrentThreadStack(void *State,
    uint64_t *StackTop, uint64_t *StackBottom)
{
  CPUState *CpuState = (CPUState*)State;
  //KPRCB32 Kprcb;
  //KTHREAD32 KThread;

  if (!QEMU::ReadVirtualMemory(CpuState->segs[R_FS].base + 0x4, StackTop, 4) ) {
    return false;
  }

  if (!QEMU::ReadVirtualMemory(CpuState->segs[R_FS].base + 0x8, StackBottom, 4) ) {
    return false;
  }

  return true;
}

bool WindowsSpy::ReadUnicodeString(std::string &Result, uint64_t Offset)
{
  uint16_t Length, MaxLength;
  uint64_t Pointer;

  if (!QEMU::ReadVirtualMemory(Offset, &Length, 2) ) {
    return false;
  }

  if (!QEMU::ReadVirtualMemory(Offset+2, &MaxLength, 2) ) {
    return false;
  }

  if (!QEMU::ReadVirtualMemory(Offset+4, &Pointer, m_OS->GetPointerSize()) ) {
    return false;
  }

  if (Length > MaxLength) {
    return false;
  }
  
  Result = QEMU::GetUnicode(Pointer, Length);

  return true;
}
