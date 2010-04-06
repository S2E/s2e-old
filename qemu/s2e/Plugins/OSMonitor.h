#ifndef __MODULE_MONITOR_PLUGIN_H__

#define __MODULE_MONITOR_PLUGIN_H__

#include <s2e/Plugin.h>

#include <s2e/Interceptor/ExecutableImage.h>

namespace s2e {
namespace plugins {

   class OSMonitor:public Plugin
   {
   public:
      sigc::signal<void, 
         const ModuleDescriptor,
         const IExecutableImage::Imports,
         const IExecutableImage::Exports
      >onModuleLoad;

      sigc::signal<void, const ModuleDescriptor> onModuleUnload;
      sigc::signal<void, uint64_t> onProcessUnload;
   protected:
      OSMonitor(S2E* s2e): Plugin(s2e) {}

   public:


   };

} // namespace plugins
} // namespace s2e

#endif