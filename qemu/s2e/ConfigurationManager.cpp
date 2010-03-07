#include "ConfigurationManager.h"
#include "config-host.h"

#include <iostream>
#include <sstream>
#include <s2e/s2e.h>


#include <unistd.h>

using namespace std;

CConfigurationManager* CConfigurationManager::s_Instance = NULL;

CConfigurationManager::CConfigurationManager()
{
  m_CurrentDir = getcwd(NULL, 0);
  m_S2ERoot = getcwd(NULL, 0);
  m_PluginPath = getcwd(NULL, 0);
}

CConfigurationManager::~CConfigurationManager()
{

}

const std::string& CConfigurationManager::GetS2ERoot() const
{
  return m_S2ERoot;
}

void CConfigurationManager::SetS2ERoot(const string &Path)
{
  m_S2ERoot = Path;
}

const std::string& CConfigurationManager::GetPluginPath() const
{
  return m_S2ERoot;
}

void CConfigurationManager::SetPluginPath(const string &Path)
{
  m_PluginPath = Path;
}


CConfigurationManager* CConfigurationManager::GetInstance()
{
  if (!s_Instance) {
    s_Instance = new CConfigurationManager();
  }
  return s_Instance;
}




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


extern "C"
{
void S2ESetConfigOption(enum ES2EOption Opt, const char *Value)
{
  CConfigurationManager *Cfg = CConfigurationManager::GetInstance();
  switch(Opt)
  {
  case S2E_OPT_ROOT_PATH:
    Cfg->SetS2ERoot(Value);
    break;

  case S2E_OPT_PLUGIN_PATH:
    Cfg->SetPluginPath(Value);
    break;

  default:
    std::cout << "Invalid S2ESetConfigOption option " << std::dec << Opt << std::endl;
    exit(-1);
  }
}
}