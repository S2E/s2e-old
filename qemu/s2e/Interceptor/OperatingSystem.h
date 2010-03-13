#ifndef _OPERATING_SYSTEM_H_

#define _OPERATING_SYSTEM_H_

#include <inttypes.h>
#include "Interceptor.h"
#include <s2e/Plugins/PluginInterface.h>
#include <s2e/Configuration/ConfigurationManager.h>

struct IOperatingSystem {
  virtual IInterceptor* GetNewInterceptor(const std::string &ModuleName, bool UserMode)=0;
};

typedef IOperatingSystem * (*OSPLUGIN_GETINSTANCE)(const char *OsType, const char *OsVer, const S2E_PLUGIN_API *Api);
typedef void (*OSPLUGIN_RELEASE)(IOperatingSystem *OS);


class COperatingSystem {
protected:
  static COperatingSystem *s_Instance;
  IOperatingSystem *m_Interface;
  void *m_Plugin;
  bool m_Loaded;
  
  bool Load();

  CConfigurationManager *m_CfgMgr;

  std::vector<IInterceptor*> m_Interceptors;
  
public:
  COperatingSystem(CConfigurationManager *Cfg);
  ~COperatingSystem();

  void SetInterface(IOperatingSystem *OS) {
    m_Interface = OS;
  }

  bool LoadModuleInterceptors();
  bool IsLoaded() const {
    return m_Loaded;
  }

  virtual bool OnTbEnter(void *CpuState, bool Translation);
  virtual bool OnTbExit(void *CpuState, bool Translation);
};


extern "C"
{
  void InitOperatingSystem(const char *OsType, const char *OsVer);
  //IOperatingSystem *CreateOSInstance(const char *OsType, const char *OsVer);
}

#endif
