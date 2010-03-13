#include "OperatingSystem.h"
#include <s2e/Configuration/ConfigurationManager.h>
#include <s2e/Plugins/PluginInterface.h>
#include <s2e/s2e.h>

#include <iostream>
#include <stdlib.h>

using namespace std;



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
    I->SetModule(Module);

  }

  return true;
}

/////////////////////////////////////////////////////

COperatingSystem *COperatingSystem::s_Instance = NULL;

COperatingSystem::COperatingSystem(CConfigurationManager *Cfg)
{
  m_Interface = NULL;
  m_CfgMgr = Cfg;
  m_Loaded = Load();
}

COperatingSystem::~COperatingSystem()
{
  OSPLUGIN_RELEASE Inst = (OSPLUGIN_RELEASE)PluginInterface::GetEntryPoint(
    m_Plugin, "Release"); 
  
  if (!Inst) {
    std::cout << "Could not relase the OS plugin " << std::endl;
    return;
  }
  Inst(m_Interface);
}


bool COperatingSystem::Load()
{
  string Plugin = m_CfgMgr->GetCfgOsPluginPath();

  void *LibHandle = PluginInterface::LoadPlugin(Plugin);
  if (!LibHandle) {
    std::cout << "Could not load " << Plugin << std::endl;
    return false;
  }

  OSPLUGIN_GETINSTANCE Inst = (OSPLUGIN_GETINSTANCE)PluginInterface::GetEntryPoint(
    LibHandle, "GetInstance");

  if (!Inst) {
    std::cout << "Could not find GetInstance entry point in " << Plugin << std::endl; 
    return false;
  }

  S2E_PLUGIN_API API;
  PluginInterface::PluginApiInit(API);
  
  IOperatingSystem *IOS = Inst(m_CfgMgr->GetCfgOsType().c_str(), 
    m_CfgMgr->GetCfgOsVersion().c_str(), &API);
  
  if (IOS) {
    m_Interface = IOS;
    m_Plugin = LibHandle;
    std::cout << "Loaded plugin for " << m_CfgMgr->GetCfgOsType() 
      << " " << m_CfgMgr->GetCfgOsVersion() << " - " << Plugin
      << std::endl;
    return true;
  }

  PluginInterface::UnloadPlugin(LibHandle);

  return false;
}


