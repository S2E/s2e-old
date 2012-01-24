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

#ifndef S2E_PLUGINS_DEBUG_H
#define S2E_PLUGINS_DEBUG_H

#include <s2e/Plugin.h>
#include <s2e/Plugins/CorePlugin.h>
#include <s2e/S2EExecutionState.h>

namespace s2e {
namespace plugins {


/**
 *  This is a plugin for aiding in debugging guest code.
 *  XXX: To be replaced by gdb.
 */
class Debugger : public Plugin
{
    S2E_PLUGIN
public:
    Debugger(S2E* s2e): Plugin(s2e) {}
    virtual ~Debugger();

    void initialize();

    struct AddressRange {
        AddressRange(uint64_t s, uint64_t e) {
            start = s;
            end = e;
        }

        uint64_t start, end;
    };


private:

    uint64_t *m_dataTriggers;
    unsigned m_dataTriggerCount;

    std::vector<AddressRange> m_addressTriggers;

    bool m_monitorStack;
    uint64_t m_catchAbove;

    uint64_t m_timeTrigger;
    uint64_t m_elapsedTics;
    sigc::connection m_timerConnection;
    sigc::connection m_memoryConnection;

    void initList(const std::string &key, uint64_t **ptr, unsigned *size);
    void initAddressTriggers(const std::string &key);

    bool dataTriggered(uint64_t data) const;
    bool addressTriggered(uint64_t address) const;

    bool decideTracing(S2EExecutionState *state, uint64_t addr, uint64_t data) const;

    void onDataMemoryAccess(S2EExecutionState *state,
                                   klee::ref<klee::Expr> address,
                                   klee::ref<klee::Expr> hostAddress,
                                   klee::ref<klee::Expr> value,
                                   bool isWrite, bool isIO);

    void onTranslateInstructionStart(
        ExecutionSignal *signal,
        S2EExecutionState *state,
        TranslationBlock *tb,
        uint64_t pc
        );

    void onInstruction(S2EExecutionState *state, uint64_t pc);

    void onTimer();

};

} // namespace plugins
} // namespace s2e

#endif // S2E_PLUGINS_EXAMPLE_H
