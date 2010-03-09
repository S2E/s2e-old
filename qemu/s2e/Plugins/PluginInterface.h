#ifndef __PLUGIN_INTERFACE_H_

#define __PLUGIN_INTERFACE_H_

#include <string>
#include <vector>
#include <inttypes.h>
#include <s2e/Interceptor/Interceptor.h>

typedef struct _S2E_PLUGIN_API {
  bool (*ReadVirtualMemory)(uint64_t Addr, void *Buffer, unsigned Length);
  uint64_t (*GetPhysAddr)(uint64_t va);
  std::string (*GetUnicode)(uint64_t base, unsigned size);
  bool (*GetAsciiz)(uint64_t base, std::string &ret);
  void (*DumpVirtualMemory)(uint64_t Addr, unsigned Length);
}S2E_PLUGIN_API;

class IOperatingSystem {
public:
  virtual bool CheckDriverLoad(uint64_t eip) = 0;
  virtual bool CheckPanic(uint64_t eip) const = 0;
  virtual IInterceptor* GetNewInterceptor(const std::string &ModuleName) = 0;
  virtual ~IOperatingSystem()=0;
};

typedef IOperatingSystem * (*OSPLUGIN_GETINSTANCE)(const char *OsType, const char *OsVer, const S2E_PLUGIN_API *Api);

void PluginApiInit(S2E_PLUGIN_API &Out);

class PluginInterface
{
public:
  typedef std::vector<std::string> PluginNameList;

private:
  static bool GetPluginNameList(PluginNameList &List, std::string &Prefix);

public:
  static void PluginApiInit(S2E_PLUGIN_API &Out);
  static void *LoadPlugin(const std::string &Name);
  static bool UnloadPlugin(void *Opaque);
  static void *GetEntryPoint(void *Opaque, const std::string &FunctionName);

  static bool GetOSPluginNameList(PluginNameList &List);
  static bool GetHandlerPluginNameList(PluginNameList &List);


};

#endif