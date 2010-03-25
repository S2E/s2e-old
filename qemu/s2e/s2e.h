#ifndef _S2E_H_
#define _S2E_H_

#ifdef __cplusplus

#include <s2e/ConfigFile.h>
#include "Interceptor/OperatingSystem.h"

class S2E
{
private:
  static S2E *s_S2E;
  ConfigFile *m_configFile;
  COperatingSystem *m_Os;

  S2E(const char *CfgFile);
  
public:

  static S2E* GetInstance(const char *CfgFile);
  static S2E* GetInstance();

  COperatingSystem *GetOS() const;

  ConfigFile* config() const {
    return m_configFile;
  }
};

#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Function declarations for QEMU */

int S2EInit(const char *CfgFile);
int S2EOnTbEnter(void *CpuState, int Translation);
int S2EOnTbExit(void *CpuState, int Translation);

#ifdef __cplusplus
}
#endif

#endif
