#ifndef S2E_H
#define S2E_H

#include <string>
#include <map>

namespace s2e {

class Plugin;
class CorePlugin;
class ConfigFile;
class PluginsFactory;

class S2E
{
private:
  static S2E *s_S2E;
  ConfigFile *m_configFile;

  PluginsFactory *m_pluginsFactory;

  CorePlugin* m_corePlugin;
  std::map<std::string, Plugin*> m_activePlugins;
  
public:
  explicit S2E(const std::string& configFileName);

  ConfigFile* getConfig() const { return m_configFile; }

  Plugin* getPlugin(const std::string& name) const;
  CorePlugin* getCorePlugin() const { return m_corePlugin; }
};

} // namespace s2e

#endif // S2E_H
