#ifndef S2E_H
#define S2E_H

#ifdef __cplusplus

#include <s2e/ConfigFile.h>
#include <string>
#include <map>

namespace s2e {

class Plugin;
class CorePlugin;
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

using s2e::S2E;

#endif // __cplusplus

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __cplusplus

struct S2E;

/* Global S2E instance. Should only be used in QEMU code */
extern struct S2E* g_s2e;

#endif

/* Function declarations for QEMU */

struct S2E* s2e_initialize(const char *s2e_config_file);
void s2e_close(struct S2E* s2e);

#ifdef __cplusplus
}
#endif

#endif // S2E_H
