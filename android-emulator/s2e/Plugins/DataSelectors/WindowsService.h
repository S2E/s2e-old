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
 *    Vitaly Chipounov <vitaly.chipounov@epfl.ch>
 *    Volodymyr Kuznetsov <vova.kuznetsov@epfl.ch>
 *
 * All contributors are listed in S2E-AUTHORS file.
 *
 */

#ifndef S2E_PLUGINS_WINDOWS_SERVICE_H
#define S2E_PLUGINS_WINDOWS_SERVICE_H

#include <s2e/Plugin.h>
#include <s2e/Plugins/CorePlugin.h>
#include <s2e/S2EExecutionState.h>

#include "DataSelector.h"
#include <s2e/Plugins/WindowsInterceptor/WindowsMonitor.h>

namespace s2e {
namespace plugins {

struct WindowsServiceCfg
{
    std::string serviceId;
    std::string moduleId;
    bool makeParamCountSymbolic;
    bool makeParamsSymbolic;
};

class WindowsService : public DataSelector
{
    S2E_PLUGIN
public:
    WindowsService(S2E* s2e): DataSelector(s2e) {}

    void initialize();
    void onTranslateBlockStart(ExecutionSignal*, S2EExecutionState *state, 
        const ModuleDescriptor &desc,
        TranslationBlock *tb, uint64_t pc);
    
    void onExecution(S2EExecutionState *state, uint64_t pc);

private:
    //Handle only one service for now
    WindowsServiceCfg m_ServiceCfg;

    WindowsMonitor *m_WindowsMonitor;
    ModuleExecutionDetector *m_executionDetector;
    std::set<std::string> m_modules;
    sigc::connection m_TbConnection;

    virtual bool initSection(const std::string &cfgKey, const std::string &svcId);
    
    
};

} // namespace plugins
} // namespace s2e

#endif // S2E_PLUGINS_EXAMPLE_H
