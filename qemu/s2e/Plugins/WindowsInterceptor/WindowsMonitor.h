#ifndef _WINDOWS_PLUGIN_H_

#define _WINDOWS_PLUGIN_H_

#include <s2e/Interceptor/ModuleDescriptor.h>
#include <s2e/Plugins/PluginInterface.h>

#include <s2e/Plugin.h>
#include <s2e/Plugins/CorePlugin.h>
#include <s2e/Plugins/OSMonitor.h>

#include "WindowsSpy.h"

#include <inttypes.h>
#include <set>
#include <map>


namespace s2e {
namespace plugins {

class WindowsMonitor:public OSMonitor
{
    S2E_PLUGIN

public:
  typedef enum EWinVer {
    SP2, SP3
  }EWinVer;

private:
    EWinVer m_Version;
    bool m_UserMode, m_KernelMode;
    unsigned m_PointerSize;
    uint64_t m_KernelBase;
public:
    WindowsMonitor(S2E* s2e): OSMonitor(s2e) {}
    void initialize();

    void slotTranslateBlockStart(ExecutionSignal *signal, uint64_t pc);
    void slotUmExecuteBlockStart(S2EExecutionState *state, uint64_t pc);
    void slotKmExecuteBlockStart(S2EExecutionState *state, uint64_t pc);

    uint64_t GetUserAddressSpaceSize() const;
    uint64_t GetKernelStart() const;
    uint64_t GetLdrpCallInitRoutine() const;
    bool CheckDriverLoad(uint64_t eip) const;
    bool CheckPanic(uint64_t eip) const;
    unsigned GetPointerSize() const;
    
};

#if 0

class WindowsPlugin:public Plugin
{
  S2E_PLUGIN

public:
  typedef enum EWinVer {
    SP2, SP3
  }EWinVer;

protected:
  WindowsPlugin(S2E* s2e);
  ~CWindowsOS();

  WindowsSpy *m_Spy;

  unsigned m_PointerSize;

private:
  EWinVer m_Version;
  uintptr_t m_KernelBase;
public:
  CWindowsOS(EWinVer WinVer);
  
  virtual IInterceptor* GetNewInterceptor(const std::string &ModuleName, bool UserMode);
//  uintptr_t GetDriverObject(void *CpuState);

  uint64_t GetUserAddressSpaceSize() const;
  uint64_t GetKernelStart() const;
  uint64_t GetLdrpCallInitRoutine() const;
  bool CheckDriverLoad(uintptr_t eip) const;
  bool CheckPanic(uintptr_t eip) const;

  unsigned GetPointerSize() const;

  WindowsSpy *GetSpy() const;

  friend void Release(IOperatingSystem *OS);

};

#endif

} // namespace plugins
} // namespace s2e


#endif