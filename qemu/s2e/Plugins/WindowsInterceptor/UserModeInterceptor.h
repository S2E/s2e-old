#ifndef __WINDOWS_UM_INTERCEPTOR_H__

#define __WINDOWS_UM_INTERCEPTOR_H__

#include <s2e/Plugin.h>
#include <s2e/Plugins/CorePlugin.h>


#include "WindowsOS.h"

class WindowsUmInterceptorPlugin: public Plugin, public sigc::trackable
{
public:
  S2E_PLUGIN

private:
  CWindowsOS *m_Os;

  typedef std::set<ModuleDescriptor*> HookedLibrarySet;
  typedef std::set<std::string> StringSet;

  uint64_t m_ASBase;
  uint64_t m_ASSize;
  
  enum EUmTracingState {
    SEARCH_PROCESS,
    WAIT_PROCESS_INIT,
    WAIT_LIBRARY_LOAD,
    LOAD_DONE
  };

  IInterceptorEvent *m_Events;

  enum EUmTracingState m_TracingState;

  uint64_t m_Cr3;
  uint64_t m_PrevCr3;
  uint64_t m_HookedCr3;
  uint64_t m_LdrAddr; 
  uint64_t m_ProcBase;
  
  ModuleDescriptor::MDSet m_LoadedLibraries;

  IDataStructureSpy::Processes m_ProcList;

  std::string m_ProcessName;
  std::map<std::string, ModuleDescriptor> m_Modules;

  bool WaitForProcessInit(void *CpuState);
  int FindModules();
  bool InitImports();

  void NotifyProcessLoad();
  void NotifyLibraryLoad(const ModuleDescriptor &Library);

public:
  WindowsUmInterceptorPlugin(S2E* s2e): Plugin(s2e) {}
  virtual ~WindowsUmInterceptorPlugin();

  virtual bool OnTbEnter(void *CpuState, bool Translation);
  virtual bool OnTbExit(void *CpuState, bool Translation);
  
  /**
   *  Called by the code translator module of the VM
   *  to see whether it has to translate the basic block to LLVM
   *  or to native binary code.
   *  The basic block is characterized by the page directory pointer
   *  (which usually identifies the process) and the program counter.
   */
  //virtual bool DecideSymbExec(uint64_t cr3, uint64_t Pc);
  virtual void DumpInfo(std::ostream &os);
  virtual bool GetModule(ModuleDescriptor &Desc);

  virtual void SetEventHandler(struct IInterceptorEvent *Hdlr);

  bool SetModule(const std::string &Name);
};




#endif