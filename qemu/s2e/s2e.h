#ifndef _S2E_H_

#define _S2E_H_

enum ES2EOption
{
  S2E_OPT_ROOT_PATH,
  S2E_OPT_PLUGIN_PATH
};

#include <stdio.h>
#include <assert.h>
#define DPRINTF(...) printf(__VA_ARGS__)

#ifdef __cplusplus

#else

int S2EInitOperatingSystem(const char *OsType, const char *OsVer);
void S2ESetConfigOption(enum ES2EOption Opt, const char *Value);
#endif

#endif