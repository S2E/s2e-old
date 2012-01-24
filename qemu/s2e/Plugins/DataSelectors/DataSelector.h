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

#ifndef S2E_PLUGINS_DATA_SELECTOR_H
#define S2E_PLUGINS_DATA_SELECTOR_H

#include <s2e/Plugin.h>
#include <s2e/Plugins/CorePlugin.h>
#include <s2e/S2EExecutionState.h>
#include <s2e/Plugins/ModuleExecutionDetector.h>

namespace s2e {
namespace plugins {

//This class contains generic methods to parse
//the constraints specified in the configuration files.
//Subclasses should implement platform-specific selector.
class DataSelector : public Plugin
{
public:
    DataSelector(S2E* s2e): Plugin(s2e) {}
    
    void initialize();
protected:
    static klee::ref<klee::Expr> getNonNullCharacter(S2EExecutionState *s, klee::Expr::Width w);
    static klee::ref<klee::Expr> getUpperBound(S2EExecutionState *s, uint64_t upperBound, klee::Expr::Width w);
    static klee::ref<klee::Expr> getOddValue(S2EExecutionState *s, klee::Expr::Width w);
    static klee::ref<klee::Expr> getOddValue(S2EExecutionState *s, klee::Expr::Width w, uint64_t upperBound);
    bool makeUnicodeStringSymbolic(S2EExecutionState *s, uint64_t address);
    bool makeStringSymbolic(S2EExecutionState *s, uint64_t address);

    virtual bool initSection(const std::string &cfgKey, const std::string &svcId) = 0;

    ModuleExecutionDetector *m_ExecDetector;
    
};

} // namespace plugins
} // namespace s2e

#endif // S2E_PLUGINS_EXAMPLE_H
