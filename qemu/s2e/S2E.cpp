#include "S2E.h"

#include <s2e/Plugin.h>
#include <s2e/Plugins/CorePlugin.h>
#include <s2e/ConfigFile.h>
#include <s2e/Utils.h>

#include <s2e/s2e_qemu.h>

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

  m_activePlugins.insert(
          make_pair(m_corePlugin->getPluginInfo()->name, m_corePlugin));
  if(!m_corePlugin->getPluginInfo()->functionName.empty())
      m_activePlugins.insert(
          make_pair(m_corePlugin->getPluginInfo()->functionName, m_corePlugin));

  vector<string> pluginNames = getConfig()->getStringList("plugins");

  /* Check and load plugins */
  foreach(const string& pluginName, pluginNames) {
     const PluginInfo* pluginInfo = m_pluginsFactory->getPluginInfo(pluginName);
     if(!pluginInfo) {
        std::cerr << "ERROR: plugin '" << pluginName
                  << "' does not exists in this S2E installation" << std::endl;
        exit(1);
    } else if(getPlugin(pluginInfo->name)) {
        std::cerr << "ERROR: plugin '" << pluginInfo->name
                  << "' was already loaded "
                  << "(is it enabled multiple times ?)" << std::endl;
        exit(1);
    } else if(!pluginInfo->functionName.empty() &&
                getPlugin(pluginInfo->functionName)) {
        std::cerr << "ERROR: plugin '" << pluginInfo->name
                  << "' with function '" << pluginInfo->functionName
                  << "' can not be loaded because" << std::endl
                  << "    this function is already provided by '"
                  << getPlugin(pluginInfo->functionName)->getPluginInfo()->name
                  << "' plugin" << std::endl;
        exit(1);
    } else {
        Plugin* plugin = m_pluginsFactory->createPlugin(this, pluginName);
        assert(plugin);

        m_activePlugins.insert(make_pair(plugin->getPluginInfo()->name, plugin));
        if(!plugin->getPluginInfo()->functionName.empty())
            m_activePlugins.insert(
                make_pair(plugin->getPluginInfo()->functionName, plugin));
    }
  }

  /* Check dependencies */
  typedef pair<string, Plugin*> PluginItem;
  foreach(const PluginItem& p, m_activePlugins) {
    foreach(const string& name, p.second->getPluginInfo()->dependencies) {
        if(!getPlugin(name)) {
            std::cerr << "ERROR: plugin '" << p.second->getPluginInfo()->name
                      << "' depends on plugin '" << name
                      << "' which is not enabled in config" << std::endl;
            exit(1);
        }
    }
  }

  /* Initialize plugins */
  foreach(const PluginItem& p, m_activePlugins) {
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

/******************************/
/* Functions called from QEMU */

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
