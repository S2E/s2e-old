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
 * All contributors are listed in the S2E-AUTHORS file.
 */

#ifndef S2E_PLUGINS_CUSTINST_H

#define S2E_PLUGINS_CUSTINST_H

#include <s2e/Plugin.h>
#include <s2e/Plugins/CorePlugin.h>
#include <s2e/S2EExecutionState.h>

namespace s2e {
namespace plugins {

class BaseInstructions : public Plugin
{
    S2E_PLUGIN
public:
    BaseInstructions(S2E* s2e): Plugin(s2e) {}

    void initialize();
   
    void handleBuiltInOps(S2EExecutionState* state, 
        uint64_t opcode);

private:
    void onCustomInstruction(S2EExecutionState* state, 
        uint64_t opcode);
    void invokePlugin(S2EExecutionState *state);
    void makeSymbolic(S2EExecutionState *state, bool makeConcolic);
    void isSymbolic(S2EExecutionState *state);
    void killState(S2EExecutionState *state);
    void printExpression(S2EExecutionState *state);
    void printMessage(S2EExecutionState *state, bool isWarning);
    void printMemory(S2EExecutionState *state);
    void concretize(S2EExecutionState *state, bool addConstraint);
    void getPreciseBound(S2EExecutionState *state, bool upper);
    void sleep(S2EExecutionState *state);
    void assume(S2EExecutionState *state);

#ifdef TARGET_ARM

#define PARAM0 CPU_OFFSET(regs[0])
#define PARAM1 CPU_OFFSET(regs[1])
#define PARAM2 CPU_OFFSET(regs[2])
#define PARAM3 CPU_OFFSET(regs[3])

#elif defined(TARGET_I386)

#define PARAM0 CPU_OFFSET(regs[R_EAX])
#define PARAM1 CPU_OFFSET(regs[R_EBX])
#define PARAM2 CPU_OFFSET(regs[R_ECX])
#define PARAM3 CPU_OFFSET(regs[R_EDX])

#else
#error "Target architecture not supported"
#endif

};

class BaseInstructionsPluginInvokerInterface {
public:
    virtual unsigned int handleOpcodeInvocation(S2EExecutionState *state,
                       	   	                    uint64_t guestDataPtr,
                                                uint64_t guestDataSize) = 0;
};

} // namespace plugins
} // namespace s2e

#endif
