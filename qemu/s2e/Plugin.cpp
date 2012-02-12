/*
 * S2E Selective Symbolic Execution Framework
 *
 * Copyright (c) 2010, Dependable Systems Laboratory, EPFL
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Dependable Systems Laboratory, EPFL nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE DEPENDABLE SYSTEMS LABORATORY, EPFL BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Currently maintained by:
 *    Volodymyr Kuznetsov <vova.kuznetsov@epfl.ch>
 *    Vitaly Chipounov <vitaly.chipounov@epfl.ch>
 *
 * All contributors are listed in S2E-AUTHORS file.
 *
 */

#include <s2e/Plugin.h>
#include <s2e/S2E.h>

// XXX: hack: for now we include and register all plugins right there
#include <s2e/Plugins/CorePlugin.h>
#include <s2e/Plugins/Example.h>
#include <s2e/Plugins/RawMonitor.h>
#include <s2e/Plugins/FunctionMonitor.h>
#include <s2e/Plugins/StackMonitor.h>
#include <s2e/Plugins/LibraryCallMonitor.h>
#include <s2e/Plugins/WindowsInterceptor/WindowsMonitor.h>
#include <s2e/Plugins/WindowsInterceptor/BlueScreenInterceptor.h>
#include <s2e/Plugins/WindowsInterceptor/WindowsCrashDumpGenerator.h>
#include <s2e/Plugins/ModuleExecutionDetector.h>
#include <s2e/Plugins/CodeSelector.h>
#include <s2e/Plugins/BaseInstructions.h>
#include <s2e/Plugins/DataSelectors/WindowsService.h>
#include <s2e/Plugins/DataSelectors/GenericDataSelector.h>
#include <s2e/Plugins/ExecutionTracers/ExecutionTracer.h>
#include <s2e/Plugins/ExecutionTracers/ModuleTracer.h>
#include <s2e/Plugins/ExecutionTracers/TestCaseGenerator.h>
#include <s2e/Plugins/ExecutionTracers/MemoryTracer.h>
#include <s2e/Plugins/ExecutionTracers/InstructionCounter.h>
#include <s2e/Plugins/ExecutionTracers/TranslationBlockTracer.h>
#include <s2e/Plugins/CacheSim.h>
#include <s2e/Plugins/Debugger.h>
#include <s2e/Plugins/SymbolicHardware.h>
#include <s2e/Plugins/EdgeKiller.h>
#include <s2e/Plugins/StateManager.h>
#include <s2e/Plugins/Annotation.h>
#include <s2e/Plugins/X86ExceptionInterceptor.h>
#include <s2e/Plugins/HostFiles.h>
#include <s2e/Plugins/MemoryChecker.h>
#include <s2e/Plugins/StackChecker.h>
#include <s2e/Plugins/InterruptInjector.h>
#include <s2e/Plugins/WindowsApi/NdisHandlers.h>
#include <s2e/Plugins/WindowsApi/NtoskrnlHandlers.h>
#include <s2e/Plugins/WindowsApi/HalHandlers.h>
#include <s2e/Plugins/WindowsApi/WindowsDriverExerciser.h>
#include <s2e/Plugins/Searchers/MaxTbSearcher.h>
#include <s2e/Plugins/Searchers/CooperativeSearcher.h>

#include <algorithm>
#include <assert.h>

namespace s2e {

using namespace std;

void Plugin::initialize()
{
}

PluginState *Plugin::getPluginState(S2EExecutionState *s, PluginStateFactory f) const
{
    if (m_CachedPluginS2EState == s) {
        return m_CachedPluginState;
    }
    m_CachedPluginState = s->getPluginState(const_cast<Plugin*>(this), f);
    m_CachedPluginS2EState = s;
    return m_CachedPluginState;
}

PluginsFactory::PluginsFactory()
{
#define __S2E_REGISTER_PLUGIN(className) \
    registerPlugin(className::getPluginInfoStatic())

    __S2E_REGISTER_PLUGIN(CorePlugin);
    __S2E_REGISTER_PLUGIN(plugins::RawMonitor);
    __S2E_REGISTER_PLUGIN(plugins::FunctionMonitor);
    __S2E_REGISTER_PLUGIN(plugins::StackMonitor);
    __S2E_REGISTER_PLUGIN(plugins::LibraryCallMonitor);
    __S2E_REGISTER_PLUGIN(plugins::WindowsMonitor);
    __S2E_REGISTER_PLUGIN(plugins::BlueScreenInterceptor);
    __S2E_REGISTER_PLUGIN(plugins::WindowsCrashDumpGenerator);
    __S2E_REGISTER_PLUGIN(plugins::ModuleExecutionDetector);
    __S2E_REGISTER_PLUGIN(plugins::CodeSelector);
    __S2E_REGISTER_PLUGIN(plugins::BaseInstructions);
    __S2E_REGISTER_PLUGIN(plugins::WindowsService);
    __S2E_REGISTER_PLUGIN(plugins::GenericDataSelector);
    __S2E_REGISTER_PLUGIN(plugins::CacheSim);
    __S2E_REGISTER_PLUGIN(plugins::InterruptInjector);

    __S2E_REGISTER_PLUGIN(plugins::ExecutionTracer);
    __S2E_REGISTER_PLUGIN(plugins::ModuleTracer);
    __S2E_REGISTER_PLUGIN(plugins::TestCaseGenerator);
    __S2E_REGISTER_PLUGIN(plugins::MemoryTracer);
    __S2E_REGISTER_PLUGIN(plugins::InstructionCounter);
    __S2E_REGISTER_PLUGIN(plugins::TranslationBlockTracer);

    __S2E_REGISTER_PLUGIN(plugins::SymbolicHardware);
    __S2E_REGISTER_PLUGIN(plugins::EdgeKiller);
    __S2E_REGISTER_PLUGIN(plugins::Annotation);
    __S2E_REGISTER_PLUGIN(plugins::X86ExceptionInterceptor);

    __S2E_REGISTER_PLUGIN(plugins::WindowsDriverExerciser);
    __S2E_REGISTER_PLUGIN(plugins::NdisHandlers);
    __S2E_REGISTER_PLUGIN(plugins::NtoskrnlHandlers);
    __S2E_REGISTER_PLUGIN(plugins::HalHandlers);

    __S2E_REGISTER_PLUGIN(plugins::StateManager);

    __S2E_REGISTER_PLUGIN(plugins::MaxTbSearcher);
    __S2E_REGISTER_PLUGIN(plugins::CooperativeSearcher);

    __S2E_REGISTER_PLUGIN(plugins::HostFiles);

    __S2E_REGISTER_PLUGIN(plugins::MemoryChecker);
    __S2E_REGISTER_PLUGIN(plugins::StackChecker);

    __S2E_REGISTER_PLUGIN(plugins::Debugger);
    __S2E_REGISTER_PLUGIN(plugins::Example);

#undef __S2E_REGISTER_PLUGIN
}

void PluginsFactory::registerPlugin(const PluginInfo* pluginInfo)
{
    assert(m_pluginsMap.find(pluginInfo->name) == m_pluginsMap.end());
    //assert(find(pluginInfo, m_pluginsList.begin(), m_pluginsList.end()) ==
      //                                              m_pluginsList.end());

    m_pluginsList.push_back(pluginInfo);
    m_pluginsMap.insert(make_pair(pluginInfo->name, pluginInfo));
}

const vector<const PluginInfo*>& PluginsFactory::getPluginInfoList() const
{
    return m_pluginsList;
}

const PluginInfo* PluginsFactory::getPluginInfo(const string& name) const
{
    PluginsMap::const_iterator it = m_pluginsMap.find(name);

    if(it != m_pluginsMap.end())
        return it->second;
    else
        return NULL;
}

Plugin* PluginsFactory::createPlugin(S2E* s2e, const string& name) const
{
    const PluginInfo* pluginInfo = getPluginInfo(name);
    s2e->getMessagesStream() << "Creating plugin " << name << std::endl;
    if(pluginInfo)
        return pluginInfo->instanceCreator(s2e);
    else
        return NULL;
}

} // namespace s2e
