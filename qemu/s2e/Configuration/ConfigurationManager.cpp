#include "ConfigurationManager.h"
#include <s2e/Plugins/PluginInterface.h>
#include "config-host.h"

#include <iostream>
#include <sstream>
#include <s2e/s2e.h>

#include <unistd.h>
#include <stdlib.h>

using namespace std;

#define S2E_ENV_ROOTDIR "S2E_ROOT_DIR"
#define S2E_ENV_PLUGINPATH "S2E_PLUGIN_PATH"


CConfigurationManager::CConfigurationManager(const char *CfgFileName)
{
  const char *e;

  m_CurrentDir = getcwd(NULL, 0);
  
  m_S2ERoot = getcwd(NULL, 0);
  if (!(e = getenv(S2E_ENV_ROOTDIR))) {
    std::cout << S2E_ENV_ROOTDIR << " not specified. Using current directory." << std::endl;
  }else {
    m_S2ERoot = e;
  }
  
  m_PluginPath = getcwd(NULL, 0);
  if (!(e = getenv(S2E_ENV_PLUGINPATH))) {
    std::cout << S2E_ENV_PLUGINPATH << " not specified. Using current directory." << std::endl;
  }else {
    m_PluginPath = e;
  }
  
  m_Cfg = new ConfigurationFile(string(CfgFileName));
}

CConfigurationManager::~CConfigurationManager()
{
  delete m_Cfg;
}

const std::string& CConfigurationManager::GetS2ERoot() const
{
  return m_S2ERoot;
}


const std::string& CConfigurationManager::GetPluginPath() const
{
  return m_S2ERoot;
}


const std::string& CConfigurationManager::GetConfigFile() const
{
  return m_ConfigFile;
}


#if 0
/**
 *  Buffer has the following format:
 *  DRIVER1.SYS=[LIB1,LIB2,..,LIBN]\n
 *  DRIVER2.SYS=[LIB1,LIB2,..,LIBN]\n
 *
 *  E.g.: Symb exec RTL8039.SYS (run concretely all its libraries)
 *  RTL8039.SYS=; 
 */
void CConfigurationManager::ParseModuleList(const string &Buffer, 
                                           CConfigurationManager::HookedModules &Modules)
{
  enum { MODULE, LIB} CurState = MODULE;
  string CurMod, CurLib;
  vector<string> Libs;
  for (unsigned i=0; i<Buffer.length(); i++) {
    switch(CurState) {
      case MODULE:
        if (Buffer[i] == '=') {
          CurState = LIB;
        }else {
          CurMod = CurMod + Buffer[i];
        }
        break;

      case LIB:
        if (Buffer[i] == ',') {
          Libs.push_back(CurLib);
          CurLib = "";
        }else if (Buffer[i] == '\n') {
          Modules[CurMod] = Libs;
          CurMod="";
          Libs.clear();
          CurState = MODULE;
        }else {
          CurLib = CurLib + Buffer[i];
        }
        break;
    }
  }
  Modules[CurMod] = Libs;
}

#endif

std::string CConfigurationManager::GetCfgOsPluginPath()
{
  string val;
  m_Cfg->GetValue(S2E_CFGMGR_CONFIGURATION, S2E_CFGMGR_CFG_OSPLUGIN, val);
  
  return PluginInterface::ConvertToFileName(GetPluginPath(), val);
}

std::string CConfigurationManager::GetCfgOsType()
{
  string val;
  m_Cfg->GetValue(S2E_CFGMGR_CONFIGURATION, S2E_CFGMGR_CFG_OSTYPE, val);
  return val;
}

std::string CConfigurationManager::GetCfgOsVersion()
{
  string val;
  m_Cfg->GetValue(S2E_CFGMGR_CONFIGURATION, S2E_CFGMGR_CFG_OSVERSION, val);
  return val;
}

void CConfigurationManager::GetCfgInterceptors(CgfInterceptors &I)
{
  unsigned i=0;

  do {
    stringstream InterceptorName;
    string Value;
    struct CfgInterceptor Ci;

    InterceptorName << "INTERCEPTOR" << std::dec << i;
    
    if (!m_Cfg->GetValue(InterceptorName.str(), "Type", Value)) {
      break;
    }
    Ci.UserMode = !Value.compare("USER");
    
    if (!m_Cfg->GetValue(InterceptorName.str(), "Module", Value)) {
      break;
    }
    Ci.ModuleName = Value;
    I.insert(I.end(), Ci);
    i++;
  }while(true);

}
