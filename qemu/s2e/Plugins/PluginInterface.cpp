#include "config-host.h"
#include "PluginInterface.h"

#include <s2e/QemuKleeGlue.h>
#include <sstream>
#include <iostream>

#ifdef CONFIG_WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#include <unistd.h>
#include <stdlib.h>
#endif

using namespace std;
using namespace s2e;

void PluginInterface::PluginApiInit(S2E_PLUGIN_API &Out)
{
  Out.ReadVirtualMemory = &QEMU::ReadVirtualMemory;
  Out.GetPhysAddr = &QEMU::GetPhysAddr;
  Out.GetUnicode = &QEMU::GetUnicode;
  Out.GetAsciiz = &QEMU::GetAsciiz;
  Out.DumpVirtualMemory = &QEMU::DumpVirtualMemory;

  Out.malloc = &malloc;
  Out.free = &free;

}

std::string PluginInterface::ConvertToFileName(const std::string &Path, 
                                                      const std::string &PluginName)
{
  stringstream s;
  
#ifdef CONFIG_WIN32
  s << Path << "\\" << PluginName << ".dll";
#else
  s << Path << "/" << PluginName << ".so";
#endif
  
  return s.str();
}

#ifdef CONFIG_WIN32

void *PluginInterface::LoadPlugin(const std::string &Name)
{
  HMODULE Mod = ::LoadLibrary(Name.c_str());
  if (!Mod) {
    std::cout << "Could not load " << Name << " - " << std::hex << "0x" <<
      GetLastError() << std::endl;
  }
  return (void*)Mod;
}

bool PluginInterface::UnloadPlugin(void *Opaque)
{
  return ::FreeLibrary((HMODULE)Opaque);
}

void *PluginInterface::GetEntryPoint(void *Opaque, const std::string &FunctionName)
{
  return (void*)::GetProcAddress((HMODULE)Opaque, FunctionName.c_str());
}

#else

void *PluginInterface::LoadPlugin(const std::string &Name)
{
  void *Mod = ::dlopen(Name.c_str(), RTLD_LAZY);
  if (!Mod) {
    std::cout << "Could not load " << Name << " - " << dlerror() << std::endl;
  }
  return (void*)Mod;
}

bool PluginInterface::UnloadPlugin(void *Opaque)
{
  return dlclose(Opaque);
}

void *PluginInterface::GetEntryPoint(void *Opaque, const std::string &FunctionName)
{
  void *ret = dlsym(Opaque, FunctionName.c_str());
  if (!ret) {
	std::cout << "Could not load " << FunctionName << " - " << dlerror() << std::endl;
  }
  return ret;
}




#endif

