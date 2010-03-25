#ifndef _OPERATING_SYSTEM_H_

#define _OPERATING_SYSTEM_H_

#include <inttypes.h>
#include "Interceptor.h"
#include <s2e/Plugins/PluginInterface.h>
#include <s2e/ConfigFile.h>

class S2E;

struct IOperatingSystem {
  virtual IInterceptor* GetNewInterceptor(const std::string &ModuleName, bool UserMode)=0;
};

typedef IOperatingSystem * (*OSPLUGIN_GETINSTANCE)(const char *OsType, const char *OsVer, const S2E_PLUGIN_API *Api);
typedef void (*OSPLUGIN_RELEASE)(IOperatingSystem *OS);

class COSEvents;

class COperatingSystem {
protected:
  static COperatingSystem *s_Instance;
  IOperatingSystem *m_Interface;
  COSEvents *m_Events;
  void *m_Plugin;
  bool m_Loaded;
  
  bool Load();

  S2E *m_s2e;

  std::vector<IInterceptor*> m_Interceptors;
  
public:
  COperatingSystem(S2E *s2e);
  ~COperatingSystem();

  void SetInterface(IOperatingSystem *OS);

  bool LoadModuleInterceptors();
  bool IsLoaded() const;

  virtual bool OnTbEnter(void *CpuState, bool Translation);
  virtual bool OnTbExit(void *CpuState, bool Translation);
};

class COSEvents:public IInterceptorEvent
{
private:
  COperatingSystem *m_Os;
public:
  COSEvents(COperatingSystem *Os);
  ~COSEvents();
  virtual void OnProcessLoad(
    struct IInterceptor *Interceptor,
    const ModuleDescriptor &Desc,
    const IExecutableImage::Imports &Imports,
    const IExecutableImage::Exports &Exports
  );

  virtual void OnLibraryLoad(
    struct IInterceptor *Interceptor,
    const ModuleDescriptor &Desc,
    const IExecutableImage::Imports &Imports,
    const IExecutableImage::Exports &Exports);
};

extern "C"
{
  void InitOperatingSystem(const char *OsType, const char *OsVer);
  //IOperatingSystem *CreateOSInstance(const char *OsType, const char *OsVer);
}

#endif
