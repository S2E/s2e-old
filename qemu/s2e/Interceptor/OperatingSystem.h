#ifndef _OPERATING_SYSTEM_H_

#define _OPERATING_SYSTEM_H_

#include <inttypes.h>
#include "Interceptor.h"
#include <s2e/Plugins/PluginInterface.h>
#include <s2e/Configuration/ConfigurationManager.h>

class COperatingSystem {
protected:
  static COperatingSystem *s_Instance;
  IOperatingSystem *m_Interface;
  void *m_Plugin;
  bool m_Loaded;
  
  bool Load();

  CConfigurationManager *m_CfgMgr;
  
public:
  COperatingSystem(CConfigurationManager *Cfg);
  ~COperatingSystem();

  void SetInterface(IOperatingSystem *OS) {
    m_Interface = OS;
  }

  bool LoadModuleInterceptors(const char *ModStr);
  bool IsLoaded() const {
    return m_Loaded;
  }
};


extern "C"
{
  void InitOperatingSystem(const char *OsType, const char *OsVer);
  //IOperatingSystem *CreateOSInstance(const char *OsType, const char *OsVer);
}

#endif
