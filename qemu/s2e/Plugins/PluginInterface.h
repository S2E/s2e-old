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
  char* (*GetAsciiz)(uint64_t base);
  void (*DumpVirtualMemory)(uint64_t Addr, unsigned Length);
  
  //This is to be used by the plugins when they wish to allocate
  //memory on the s2e's heap.
  void (*free)(void *mem);
  void *(*malloc)(size_t size);
}S2E_PLUGIN_API;


void PluginApiInit(S2E_PLUGIN_API &Out);

class PluginInterface
{
public:

private:
  //static bool GetPluginNameList(PluginNameList &List, std::string &Prefix);

public:
  static std::string ConvertToFileName(const std::string &Path, 
    const std::string &PluginName);
  static void PluginApiInit(S2E_PLUGIN_API &Out);
  static void *LoadPlugin(const std::string &Name);
  static bool UnloadPlugin(void *Opaque);
  static void *GetEntryPoint(void *Opaque, const std::string &FunctionName);

};


#endif
