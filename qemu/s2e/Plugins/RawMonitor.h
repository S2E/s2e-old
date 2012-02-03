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

#ifndef _RAWMONITOR_PLUGIN_H_

#define _RAWMONITOR_PLUGIN_H_

#include <s2e/Plugins/ModuleDescriptor.h>

#include <s2e/Plugin.h>
#include <s2e/Plugins/CorePlugin.h>
#include <s2e/Plugins/OSMonitor.h>

#include <vector>

namespace s2e {
namespace plugins {

class RawMonitor:public OSMonitor
{
    S2E_PLUGIN

public:
    struct Cfg {
        std::string name;
        uint64_t start;
        uint64_t size;
        uint64_t nativebase;
        uint64_t entrypoint;
        bool delayLoad;
        bool kernelMode;
    };

    struct OpcodeModuleConfig {
        uint32_t name;
        uint64_t nativeBase;
        uint64_t loadBase;
        uint64_t entryPoint;
        uint64_t size;
        uint32_t kernelMode;
    } __attribute__((packed));

    typedef std::vector<Cfg> CfgList;
private:
    CfgList m_cfg;
    sigc::connection m_onTranslateInstruction;

    uint64_t m_kernelStart;

    Imports m_imports;

    bool initSection(const std::string &cfgKey, const std::string &svcId);
    void onCustomInstruction(S2EExecutionState* state, uint64_t opcode);
    void loadModule(S2EExecutionState *state, const Cfg &c, bool delay);

    void opLoadConfiguredModule(S2EExecutionState *state);
    void opCreateImportDescriptor(S2EExecutionState *state);
    void opLoadModule(S2EExecutionState *state);

public:
    RawMonitor(S2E* s2e): OSMonitor(s2e) {}
    virtual ~RawMonitor();
    void initialize();

    void onTranslateInstructionStart(ExecutionSignal *signal,
                                     S2EExecutionState *state,
                                     TranslationBlock *tb,
                                     uint64_t pc);

    virtual bool getImports(S2EExecutionState *s, const ModuleDescriptor &desc, Imports &I);
    virtual bool getExports(S2EExecutionState *s, const ModuleDescriptor &desc, Exports &E);
    virtual bool isKernelAddress(uint64_t pc) const;
    virtual uint64_t getPid(S2EExecutionState *s, uint64_t pc);

    virtual bool getCurrentStack(S2EExecutionState *state, uint64_t *base, uint64_t *size) {
        return false;
    }
};



} // namespace plugins
} // namespace s2e


#endif
