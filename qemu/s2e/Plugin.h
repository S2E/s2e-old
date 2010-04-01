#ifndef S2E_PLUGIN_H
#define S2E_PLUGIN_H

#include <string>
#include <vector>
#include <map>

namespace s2e {

class S2E;
struct PluginInfo;

class Plugin {
private:
    S2E* m_s2e;

public:
    Plugin(S2E* s2e) : m_s2e(s2e) {}
    virtual ~Plugin() {}

    /** Return assosiated S2E instance */
    S2E* s2e() { return m_s2e; }

    /** Initialize plugin. This function is called on initialization
        after all plugin instances have already be instantied */
    virtual void initialize();

    /** Return PluginInfo for this class. Defined by S2E_PLUGIN macro */
    virtual const PluginInfo* getPluginInfo() const = 0;

    /** Return configuration key for this plugin */
    const std::string& getConfigKey() const;

};

struct PluginInfo {
    /** Unique name of the plugin */
    std::string name;

    /** Configuration key for this plugin */
    std::string configKey;

    /** Human-readable description of the plugin */
    std::string description;

    /** TODO: Dependencies */

    /** A function to create a plugin instance */
    Plugin* (*instanceCreator)(S2E*);
};

class PluginsFactory {
private:
    std::map<std::string, const PluginInfo*> m_pluginsMap;
    std::vector<const PluginInfo*> m_pluginsList;

public:
    PluginsFactory();

    void registerPlugin(const PluginInfo* pluginInfo);

    const std::vector<const PluginInfo*> &getPluginInfoList() const;
    const PluginInfo* getPluginInfo(const std::string& name) const;

    Plugin* createPlugin(S2E* s2e, const std::string& name) const;
};

/** Should be put at the begining of any S2E plugin */
#define S2E_PLUGIN                                                                 \
    private:                                                                       \
        static const PluginInfo s_pluginInfo;                                      \
    public:                                                                        \
        virtual const PluginInfo* getPluginInfo() const { return &s_pluginInfo; }  \
        static  const PluginInfo* getPluginInfoStatic() { return &s_pluginInfo; }  \
    private:

#define S2E_DEFINE_PLUGIN(className, description)                                  \
    const PluginInfo className::s_pluginInfo = {                                   \
        #className, "pluginsConfig['" #className "']", description,                \
        _pluginCreatorHelper<className>                                            \
    }

template<class C>
Plugin* _pluginCreatorHelper(S2E* s2e) { return new C(s2e); }

inline const std::string& Plugin::getConfigKey() const {
    return getPluginInfo()->configKey;
}

} // namespace s2e

#endif // S2E_PLUGIN_H
