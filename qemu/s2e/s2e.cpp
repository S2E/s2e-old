#include "S2E.h"

#include <s2e/Plugin.h>
#include <s2e/CorePlugin.h>

#include <stdlib.h>

using namespace std;

S2E* s2e = NULL;

/**********************************/
/* Function declarations for QEMU */

extern "C"
{

S2E* s2e_initialize(const char *s2e_config_file)
{
    return new S2E(s2e_config_file ? s2e_config_file : "");
}

void s2e_close(S2E *s2e)
{
    delete s2e;
}

}

/**********************************/

S2E* S2E::GetInstance()
{
  return s2e;
}

S2E::S2E(const string& configFileName)
{
  m_configFile = new ConfigFile(configFileName);

  m_pluginsFactory = new PluginsFactory();
  m_corePlugin = dynamic_cast<CorePlugin*>(
          m_pluginsFactory->createPlugin(this, "CorePlugin"));

  Plugin* examplePlugin = m_pluginsFactory->createPlugin(this, "ExamplePlugin");
  examplePlugin->initialize();
}

COperatingSystem *S2E::GetOS() const
{
  return m_Os;
}
