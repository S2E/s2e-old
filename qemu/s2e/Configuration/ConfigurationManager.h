#ifndef _S2E_CONFIG_H_

#define _S2E_CONFIG_H_

#include <string>
#include <vector>
#include <map>

#include "ConfigurationFile.h"

#define S2E_CFGMGR_CONFIGURATION "CONFIGURATION"
#define S2E_CFGMGR_CFG_OSPLUGIN "OsPlugin"
#define S2E_CFGMGR_CFG_OSTYPE "OsType"
#define S2E_CFGMGR_CFG_OSVERSION "OsVersion"

struct CfgInterceptor
{
  std::string ModuleName;
  bool UserMode;
};

typedef std::vector<struct CfgInterceptor> CgfInterceptors; 

/**
 *  Centralizes all configuration parameters
 */
class CConfigurationManager {
public:
  //typedef std::map<std::string, std::vector<std::string> > HookedModules;

private:

  std::string m_S2ERoot;
  std::string m_PluginPath;
  std::string m_CurrentDir;
  std::string m_ConfigFile;

  

  ConfigurationFile *m_Cfg;
  std::string m_CfgFile;
public:
  CConfigurationManager(const char *CfgFileName);
  ~CConfigurationManager();

  const std::string& GetS2ERoot() const;
  const std::string& GetPluginPath() const;
  const std::string& GetConfigFile() const;
  
  
  //static void ParseModuleList(const std::string &Buffer, 
  //                            HookedModules &Modules);

  std::string GetCfgOsPluginPath();
  std::string GetCfgOsType();
  std::string GetCfgOsVersion();
  void GetCfgInterceptors(CgfInterceptors &I);
};

#endif