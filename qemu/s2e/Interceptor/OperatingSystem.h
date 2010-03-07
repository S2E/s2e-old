#ifndef _OPERATING_SYSTEM_H_

#define _OPERATING_SYSTEM_H_

#include <inttypes.h>
#include "Interceptor.h"
#include <s2e/Plugins/PluginInterface.h>


class COperatingSystem {
protected:
  static COperatingSystem *s_Instance;
  IOperatingSystem *m_Interface;

  COperatingSystem();
public:

  static COperatingSystem *GetInstance();

  void SetInterface(IOperatingSystem *OS) {
    m_Interface = OS;
  }

  bool CheckDriverLoad(uintptr_t eip);
  bool CheckPanic(uintptr_t eip) const;

  bool LoadModuleInterceptors(const char *ModStr);
};


extern "C"
{
  void InitOperatingSystem(const char *OsType, const char *OsVer);
  //IOperatingSystem *CreateOSInstance(const char *OsType, const char *OsVer);
}

#endif
