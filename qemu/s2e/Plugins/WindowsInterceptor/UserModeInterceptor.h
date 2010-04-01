#ifndef __WINDOWS_UM_INTERCEPTOR_H__

#define __WINDOWS_UM_INTERCEPTOR_H__

#include <s2e/Interceptor/ModuleDescriptor.h>
#include <s2e/Plugins/PluginInterface.h>

#include <s2e/Plugin.h>
#include <s2e/Plugins/CorePlugin.h>
#include <s2e/Plugins/OSMonitor.h>


#include "WindowsMonitor.h"

namespace s2e {
namespace plugins {

class WindowsUmInterceptor
{
private:
  WindowsMonitor *m_Os;

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
  WindowsUmInterceptor(WindowsMonitor *Monitor);
  virtual ~WindowsUmInterceptor();

  virtual bool OnTbEnter(void *CpuState, bool Translation);
  
  
  
};


}
}

#endif