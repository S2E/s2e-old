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

#ifndef S2E_PLUGINS_ConsistencyModels_H
#define S2E_PLUGINS_ConsistencyModels_H

#include <s2e/Plugin.h>
#include <s2e/Plugins/CorePlugin.h>
#include <s2e/S2EExecutionState.h>

#include <stack>

namespace s2e {
namespace plugins {

enum ExecutionConsistencyModel {
    NONE, OVERCONSTR, STRICT, LOCAL, OVERAPPROX
};


class ConsistencyModelsState:public PluginState
{
private:
    ExecutionConsistencyModel m_defaultModel;
    std::stack<ExecutionConsistencyModel> m_models;
public:

    ConsistencyModelsState(ExecutionConsistencyModel model) {
        m_defaultModel = model;
    }

    virtual ~ConsistencyModelsState() {}

    virtual ConsistencyModelsState* clone() const {
        return new ConsistencyModelsState(*this);
    }

    static PluginState *factory(Plugin *p, S2EExecutionState *s);

    ExecutionConsistencyModel get() const {
        if (m_models.size() == 0) {
            return m_defaultModel;
        }
        return m_models.top();
    }

    void push(ExecutionConsistencyModel model) {
        m_models.push(model);
    }

    ExecutionConsistencyModel pop() {
        if (m_models.size() == 0) {
            return m_defaultModel;
        }
        ExecutionConsistencyModel model = m_models.top();
        m_models.pop();
        return model;
    }

};


class ConsistencyModels : public Plugin
{
    S2E_PLUGIN
public:
    ConsistencyModels(S2E* s2e): Plugin(s2e) {}

    void initialize();

    static ExecutionConsistencyModel fromString(const std::string &model);
    ExecutionConsistencyModel getDefaultModel() const {
        return m_defaultModel;
    }

    void push(S2EExecutionState *state, ExecutionConsistencyModel model) {
        DECLARE_PLUGINSTATE(ConsistencyModelsState, state);
        plgState->push(model);
    }

    ExecutionConsistencyModel pop(S2EExecutionState *state) {
        DECLARE_PLUGINSTATE(ConsistencyModelsState, state);
        return plgState->pop();
    }

    ExecutionConsistencyModel get(S2EExecutionState *state) {
        DECLARE_PLUGINSTATE(ConsistencyModelsState, state);
        return plgState->get();
    }

private:
    ExecutionConsistencyModel m_defaultModel;
};

} // namespace plugins
} // namespace s2e

#endif // S2E_PLUGINS_ConsistencyModels_H
