#ifndef _S2E_H_
#define _S2E_H_

#ifdef __cplusplus

#include <s2e/ConfigFile.h>
#include "Interceptor/OperatingSystem.h"

class CorePlugin;
class PluginsFactory;

class S2E
{
private:
  static S2E *s_S2E;
  ConfigFile *m_configFile;
  COperatingSystem *m_Os;

  PluginsFactory *m_pluginsFactory;
  CorePlugin *m_corePlugin;

  
public:
  explicit S2E(const char *s2e_config_file);

  static S2E* GetInstance();

  COperatingSystem *GetOS() const;

  ConfigFile* getConfig() const {
    return m_configFile;
  }

  CorePlugin *getCorePlugin() const {
      return m_corePlugin;
  }
};

#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __cplusplus
/* Global S2E instance. Should only be used in QEMU code */
struct S2E;
extern struct S2E* s2e;
#endif

/* Function declarations for QEMU */

struct S2E* s2e_initialize(const char *s2e_config_file);
void s2e_close(struct S2E* s2e);

int S2EOnTbEnter(void *CpuState, int Translation);
int S2EOnTbExit(void *CpuState, int Translation);

#ifdef __cplusplus
}
#endif

#endif
