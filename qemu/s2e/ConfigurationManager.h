#ifndef _S2E_CONFIG_H_

#define _S2E_CONFIG_H_

#include <string>
#include <vector>
#include <map>

/**
 *  Centralizes all configuration parameters
 */
class CConfigurationManager {
public:
  typedef std::map<std::string, std::vector<std::string> > HookedModules;

private:
  static CConfigurationManager *s_Instance;

  std::string m_S2ERoot;
  std::string m_PluginPath;
  std::string m_CurrentDir;

  

  CConfigurationManager();
  ~CConfigurationManager();
public:
  
  const std::string& GetS2ERoot() const;
  void SetS2ERoot(const std::string& S);
  const std::string& GetPluginPath() const;
  void SetPluginPath(const std::string& S);


  static CConfigurationManager* GetInstance();

  
  static void ParseModuleList(const std::string &Buffer, 
                              HookedModules &Modules);
};

#endif