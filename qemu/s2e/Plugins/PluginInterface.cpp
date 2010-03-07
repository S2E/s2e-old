#include "config-host.h"
#include "PluginInterface.h"
#include "s2e/ConfigurationManager.h"

#include <s2e/QemuKleeGlue.h>
#include <sstream>

#ifdef CONFIG_WIN32
#include <windows.h>
#endif

using namespace std;

void PluginInterface::PluginApiInit(S2E_PLUGIN_API &Out)
{
  Out.ReadVirtualMemory = &QEMU::ReadVirtualMemory;
  Out.GetPhysAddr = &QEMU::GetPhysAddr;
  Out.GetUnicode = &QEMU::GetUnicode;
  Out.GetAsciiz = &QEMU::GetAsciiz;
  Out.DumpVirtualMemory = &QEMU::DumpVirtualMemory;
}

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


bool PluginInterface::GetPluginNameList(PluginNameList &List, std::string &Prefix)
{
  WIN32_FIND_DATA ffd;
  HANDLE hHandle;
  string FileName;
  
  string s2e_root = CConfigurationManager::GetInstance()->GetS2ERoot();

  FileName = s2e_root;
  FileName += "\\*.dll";

  hHandle = FindFirstFile(FileName.c_str(), &ffd);
  if (hHandle == INVALID_HANDLE_VALUE) {
    return (GetLastError() == ERROR_FILE_NOT_FOUND);
  }

  do {
    if (!(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
      FileName = ffd.cFileName;
      if (FileName.find(Prefix) == 0) {
        stringstream Path;
        Path << s2e_root << "\\" << ffd.cFileName;
        List.push_back(Path.str());
      }
    }
  }while(FindNextFile(hHandle, &ffd) != 0);
  
  FindClose(hHandle);
  return true;
}

bool PluginInterface::GetOSPluginNameList(PluginNameList &List)
{
  string p = "osplg_";
  return GetPluginNameList(List, p);
}

bool PluginInterface::GetHandlerPluginNameList(PluginNameList &List)
{
  string p = "hdlrplg_";
  return GetPluginNameList(List, p);
}
