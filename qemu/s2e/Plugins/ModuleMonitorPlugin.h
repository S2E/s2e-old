#ifndef __MODULE_MONITOR_PLUGIN_H__

#define __MODULE_MONITOR_PLUGIN_H__

#include <s2e/Plugin.h>

class ModuleMonitorPlugin:public Plugin, public sigc::trackable
{
  S2E_PLUGIN
public:
  sigc::signal<void, 
    const ModuleDescriptor &Desc,
    const IExecutableImage::Imports &Imports,
    const IExecutableImage::Exports &Exports
  >onProcessLoad;

  sigc::signal<void, 
    const ModuleDescriptor &Desc,
    const IExecutableImage::Imports &Imports,
    const IExecutableImage::Exports &Exports
  >onLibraryLoad;

protected:
  ModuleMonitorPlugin(S2E* s2e): Plugin(s2e) {}

public:


}

#endif