#ifndef _S2E_H_

#define _S2E_H_





#ifdef __cplusplus

#include <stdio.h>
#include <assert.h>
#define DPRINTF(...) printf(__VA_ARGS__)

#define foreach(_i, _b, _e) \
	  for(typeof(_b) _i = _b, _i ## end = _e; _i != _i ## end;  ++ _i)

#include "Configuration/ConfigurationManager.h"
#include "Interceptor/OperatingSystem.h"

class S2E
{
private:
  static S2E *s_S2E;
  CConfigurationManager *m_CfgMgr;
  COperatingSystem *m_Os;

  S2E(const char *CfgFile);
  
public:

  static S2E* GetInstance(const char *CfgFile);
  static S2E* GetInstance();

  COperatingSystem *GetOS() const;
};


#else

int S2EInit(const char *CfgFile);
int S2EOnTbEnter(void *CpuState, int Translation);
int S2EOnTbExit(void *CpuState, int Translation);

#endif

#endif