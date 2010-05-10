#ifndef __WINDOWS_KM_INTERCEPTOR_H__

#define __WINDOWS_KM_INTERCEPTOR_H__

#include <s2e/Interceptor/ModuleDescriptor.h>
#include <s2e/Plugins/PluginInterface.h>

#include <s2e/Plugin.h>
#include <s2e/Plugins/CorePlugin.h>
#include <s2e/Plugins/OSMonitor.h>


#include "WindowsMonitor.h"

namespace s2e {
namespace plugins {

class WindowsKmInterceptor
{
private:
  WindowsMonitor *m_Os;

  bool GetDriverDescriptor(uint64_t pDriverObject, ModuleDescriptor &Desc);
  void NotifyDriverLoad(S2EExecutionState *state, ModuleDescriptor &Desc);
  void NotifyDriverUnload(S2EExecutionState *state, const ModuleDescriptor &Desc);
public:
  WindowsKmInterceptor(WindowsMonitor *Monitor);
  virtual ~WindowsKmInterceptor();

  bool CatchModuleLoad(S2EExecutionState *state);
  bool CatchModuleUnload(S2EExecutionState *state);
  bool ReadModuleList(S2EExecutionState *state);

  
};


}
}

#endif
