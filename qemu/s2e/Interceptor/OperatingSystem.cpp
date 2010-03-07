#include "OperatingSystem.h"
#include <s2e/ConfigurationManager.h>
#include <s2e/Plugins/PluginInterface.h>
#include <s2e/s2e.h>

#include <iostream>
#include <stdlib.h>

using namespace std;


/////////////////////////////////////////////////////
extern "C" {

int S2EInitOperatingSystem(const char *OsType, const char *OsVer)
{
  PluginInterface::PluginNameList OSPlugins;

  if (!PluginInterface::GetOSPluginNameList(OSPlugins)) {
    std::cout << "Could not get a list of OS plugins" << std::endl << std::flush;
    return -1;
  }

  if (OSPlugins.empty()) {
    std::cout << "There are no OS plugins available" << std::endl;
    return -1;
  }

  PluginInterface::PluginNameList::iterator it;
  for(it = OSPlugins.begin(); it != OSPlugins.end(); ++it) {
    void *LibHandle = PluginInterface::LoadPlugin(*it);
    if (!LibHandle) {
      std::cout << "Could not load " << *it << std::endl;
    }

    OSPLUGIN_GETINSTANCE Inst = (OSPLUGIN_GETINSTANCE)PluginInterface::GetEntryPoint(
      LibHandle, "GetInstance");

    if (!Inst) {
      std::cout << "Could not find GetInstance entry point in " << *it << std::endl; 
      continue;
    }

    S2E_PLUGIN_API API;
    PluginInterface::PluginApiInit(API);
    if (IOperatingSystem *IOS = Inst(OsType, OsVer, &API)) {
      COperatingSystem *Os = COperatingSystem::GetInstance();
      Os->SetInterface(IOS);
      return 0;
    }
    
    PluginInterface::UnloadPlugin(LibHandle);
  }

  std::cout << "No valid OS plugin libraries were found" << std::endl;
  return -1;

}

}


bool COperatingSystem::LoadModuleInterceptors(const char *ModStr)
{
  if (!m_Interface) {
    std::cout << "An OS type and version has to be set before setting module interceptors!" << std::endl;
    return false;
  }

  CConfigurationManager::HookedModules Hm;
  CConfigurationManager::ParseModuleList(ModStr, Hm);

  CConfigurationManager::HookedModules::iterator it;
  for (it = Hm.begin(); it != Hm.end(); ++it) {
    std::string Module = (*it).first;
    IInterceptor *I = m_Interface->GetNewInterceptor(Module);
    if (!I) {
      std::cout << "Could not create interceptor for " << Module << std::endl;
    }
  }

  return true;
}

/////////////////////////////////////////////////////

COperatingSystem *COperatingSystem::s_Instance = NULL;

COperatingSystem::COperatingSystem()
{
  m_Interface = NULL;
}

COperatingSystem *COperatingSystem::GetInstance()
{
  if (!s_Instance) {
    s_Instance = new COperatingSystem();
  }
  return s_Instance;
}

bool COperatingSystem::CheckDriverLoad(uintptr_t eip)
{
  if (!m_Interface){
    return false;
  }
  return m_Interface->CheckDriverLoad(eip);
}

bool COperatingSystem::CheckPanic(uintptr_t eip) const
{
  return m_Interface->CheckPanic(eip);
}


