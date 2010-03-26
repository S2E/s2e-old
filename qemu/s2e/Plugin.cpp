#include <s2e/Plugin.h>

// XXX: hack: for now we include and register all plugins right there
#include <s2e/CorePlugin.h>

#include <algorithm>
#include <assert.h>

PluginsFactory::PluginsFactory()
{
#define __S2E_REGISTER_PLUGIN(className) \
    registerPlugin(className::getPluginInfoStatic())

    __S2E_REGISTER_PLUGIN(CorePlugin);

#undef __S2E_REGISTER_PLUGIN
}

void PluginsFactory::registerPlugin(const PluginInfo* pluginInfo)
{
    assert(m_pluginsMap.find(pluginInfo->name) == m_pluginsMap.end());
    assert(std::find(m_pluginsList.begin(), m_pluginsList.end()) ==
                                                    m_plugins.end());

    m_pluginsList.append(pluginInfo);
    m_pluginsMap.insert(std::make_pair(pluginInfo->name, pluginInfo));
}

const std::vector<const PluginInfo*> PluginsFactory::getPluginInfoList() const
{
    return m_pluginsList;
}

const PluginInfo* PluginsFactory::getPluginInfo(const std::string& name) const
{
    std::map<std::string, const PluginInfo*>::iterator it =
                                m_pluginsMap.find(name);

    if(it != m_pluginsMap.end())
        return it->second;
    else
        return NULL;
}

Plugin* PluginsFactory::createPlugin(const std::string& name, S2E* s2e)
{
    const PluginInfo* pluginInfo = getPluginInfo(name);
    if(pluginInfo)
        return pluginInfo->instanceCreator(s2e);
    else
        return NULL;
}
