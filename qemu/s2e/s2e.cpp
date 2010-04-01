#include "S2E.h"

#include <s2e/Plugin.h>
#include <s2e/CorePlugin.h>
#include <s2e/Utils.h>

#include <iostream>
#include <stdlib.h>
#include <assert.h>

namespace s2e {

using namespace std;

S2E::S2E(const string& configFileName)
{
  m_configFile = new s2e::ConfigFile(configFileName);

  m_pluginsFactory = new PluginsFactory();

  m_corePlugin = dynamic_cast<CorePlugin*>(
          m_pluginsFactory->createPlugin(this, "CorePlugin"));
  assert(m_corePlugin);
  m_activePlugins.insert(make_pair(string("CorePlugin"), m_corePlugin));

  vector<string> pluginNames = getConfig()->getStringList("plugins");
  foreach(const string& pluginName, pluginNames) {
    if(m_activePlugins.find(pluginName) != m_activePlugins.end()) {
        std::cerr << "WARNING: plugin '" << pluginName
                  << "' was already loaded "
                  << "(is it enabled multiple times ?)" << std::endl;
        continue;
    }

    Plugin* plugin = m_pluginsFactory->createPlugin(this, pluginName);
    if(plugin) {
        m_activePlugins.insert(make_pair(pluginName, plugin));
    } else {
        std::cerr << "WARNING: plugin '" << pluginName
                  << "' does not exists in this S2E installation" << std::endl;
    }
  }

  typedef pair<string, Plugin*> PluginPair;
  foreach(const PluginPair& p, m_activePlugins) {
      p.second->initialize();
  }
}

Plugin* S2E::getPlugin(const std::string& name) const
{
    map<string, Plugin*>::const_iterator it = m_activePlugins.find(name);
    if(it != m_activePlugins.end())
        return const_cast<Plugin*>(it->second);
    else
        return NULL;
}

} // namespace s2e

/*************************/
/* Declarations for QEMU */

extern "C" {

S2E* g_s2e = NULL;

S2E* s2e_initialize(const char *s2e_config_file)
{
    return new S2E(s2e_config_file ? s2e_config_file : "");
}

void s2e_close(S2E *s2e)
{
    delete s2e;
}

} // extern "C"
