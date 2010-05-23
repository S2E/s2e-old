#ifndef __MODULE_MONITOR_PLUGIN_H__

#define __MODULE_MONITOR_PLUGIN_H__

#include <s2e/Plugin.h>
#include <s2e/S2EExecutionState.h>
#include "ModuleDescriptor.h"


namespace s2e {
namespace plugins {

class OSMonitor:public Plugin
{
public:
   sigc::signal<void,
      S2EExecutionState*,
      const ModuleDescriptor &
   >onModuleLoad;

   sigc::signal<void, S2EExecutionState*, const ModuleDescriptor &> onModuleUnload;
   sigc::signal<void, S2EExecutionState*, uint64_t> onProcessUnload;
protected:
   OSMonitor(S2E* s2e): Plugin(s2e) {}

public:
   virtual bool getImports(S2EExecutionState *s, const ModuleDescriptor &desc, Imports &I) = 0;
   virtual bool getExports(S2EExecutionState *s, const ModuleDescriptor &desc, Exports &E) = 0;
   virtual bool isKernelAddress(uint64_t pc) const = 0;
   virtual uint64_t getPid(S2EExecutionState *s, uint64_t pc) = 0;
};

} // namespace plugins
} // namespace s2e

#endif
