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

#include "ConsistencyModels.h"
#include <s2e/S2E.h>
#include <s2e/ConfigFile.h>
#include <s2e/Utils.h>

#include <iostream>

namespace s2e {
namespace plugins {

S2E_DEFINE_PLUGIN(ConsistencyModels, "Central manager for execution consistency models", "",);

void ConsistencyModels::initialize()
{
    ConfigFile *cfg = s2e()->getConfig();
    bool ok = false;

    std::string consistency = cfg->getString(getConfigKey() + ".model", "", &ok);
    m_defaultModel = fromString(consistency);

    if (m_defaultModel == NONE) {
        s2e()->getWarningsStream() << "ConsistencyModels: invalid consistency " << consistency << "\n";
        exit(-1);
    }
}

ExecutionConsistencyModel ConsistencyModels::fromString(const std::string &model)
{
    ExecutionConsistencyModel ret = NONE;
    //Check the consistency type
    if (model == "strict") {
        ret = STRICT;
    }else if (model == "local") {
        ret = LOCAL;
    }else if (model == "overapproximate") {
        ret = OVERAPPROX;
    }else if  (model == "overconstrained") {
        ret = OVERCONSTR;
    }

    return ret;
}

PluginState *ConsistencyModelsState::factory(Plugin *p, S2EExecutionState *s) {
    ConsistencyModels *models = static_cast<ConsistencyModels*>(p);
    return new ConsistencyModelsState(models->getDefaultModel());
}


} // namespace plugins
} // namespace s2e
