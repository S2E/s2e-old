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

extern "C" {
#include "config.h"
#include "qemu-common.h"
}

#ifdef CONFIG_WIN32
#include <windows.h>
#endif

#include "BaseInstructions.h"
#include <s2e/S2E.h>
#include <s2e/S2EExecutor.h>
#include <s2e/S2EExecutionState.h>
#include <s2e/ConfigFile.h>
#include <s2e/Utils.h>

#include <iostream>
#include <sstream>

#include <llvm/Support/TimeValue.h>
#include <klee/Searcher.h>
#include <klee/Solver.h>

#include <llvm/Support/CommandLine.h>

extern llvm::cl::opt<bool> ConcolicMode;

namespace s2e {
namespace plugins {

using namespace std;
using namespace klee;

S2E_DEFINE_PLUGIN(BaseInstructions, "Default set of custom instructions plugin", "",);

void BaseInstructions::initialize()
{
    s2e()->getCorePlugin()->onCustomInstruction.connect(
            sigc::mem_fun(*this, &BaseInstructions::onCustomInstruction));

}

void BaseInstructions::makeSymbolic(S2EExecutionState *state, bool makeConcolic)
{
    target_ulong address, size, name;
    bool ok = true;
    ok &= state->readCpuRegisterConcrete(CPU_OFFSET(regs[R_EAX]),
                                         &address, sizeof address);
    ok &= state->readCpuRegisterConcrete(CPU_OFFSET(regs[R_EBX]),
                                         &size, sizeof size);
    ok &= state->readCpuRegisterConcrete(CPU_OFFSET(regs[R_ECX]),
                                         &name, sizeof name);

    if(!ok) {
        s2e()->getWarningsStream(state)
            << "ERROR: symbolic argument was passed to s2e_op "
               " insert_symbolic opcode\n";
        return;
    }

    std::string nameStr = "unnamed";
    if(name && !state->readString(name, nameStr)) {
        s2e()->getWarningsStream(state)
                << "Error reading string from the guest\n";
    }

    s2e()->getMessagesStream(state)
            << "Inserting symbolic data at " << hexval(address)
            << " of size " << hexval(size)
            << " with name '" << nameStr << "'\n";

    std::vector<unsigned char> concreteData;
    vector<ref<Expr> > symb;

    if (makeConcolic) {
        for (unsigned i = 0; i< size; ++i) {
            uint8_t byte = 0;
            if (!state->readMemoryConcrete8(address + i, &byte)) {
                s2e()->getWarningsStream(state)
                    << "Can not concretize/read symbolic value"
                    << " at " << hexval(address + i) << ". System state not modified.\n";
                return;
            }
            concreteData.push_back(byte);
        }
        symb = state->createConcolicArray(nameStr, size, concreteData);
    } else {
        symb = state->createSymbolicArray(nameStr, size);
    }


    for(unsigned i = 0; i < size; ++i) {
        if(!state->writeMemory8(address + i, symb[i])) {
            s2e()->getWarningsStream(state)
                << "Can not insert symbolic value"
                << " at " << hexval(address + i)
                << ": can not write to memory\n";
        }
    }
}

void BaseInstructions::isSymbolic(S2EExecutionState *state)
{
    target_ulong address;
    target_ulong size;
    target_ulong result;

    bool ok = true;
    ok &= state->readCpuRegisterConcrete(CPU_OFFSET(regs[R_ECX]),
                                         &address, sizeof(address));

    ok &= state->readCpuRegisterConcrete(CPU_OFFSET(regs[R_EAX]),
                                         &size, sizeof(size));

    if(!ok) {
        s2e()->getWarningsStream(state)
            << "ERROR: symbolic argument was passed to s2e_op is_symbolic\n";
        return;
    }

    // readMemoryConcrete fails if the value is symbolic
    result = 0;
    for (unsigned i=0; i<size; ++i) {
        klee::ref<klee::Expr> ret = state->readMemory8(address + i);
        if (!isa<ConstantExpr>(ret)) {
            result = 1;
        }
    }

    s2e()->getMessagesStream(state)
            << "Testing whether data at " << hexval(address)
            << " and size " << size << " is symbolic: "
            << (result ? " true" : " false") << '\n';

    state->writeCpuRegisterConcrete(CPU_OFFSET(regs[R_EAX]), &result, sizeof(result));
}

void BaseInstructions::killState(S2EExecutionState *state)
{
    std::string message;
    target_ulong messagePtr;

#ifdef TARGET_X86_64
    const klee::Expr::Width width = klee::Expr::Int64;
#else
    const klee::Expr::Width width = klee::Expr::Int32;
#endif

    bool ok = true;
    klee::ref<klee::Expr> status =
                                state->readCpuRegister(CPU_OFFSET(regs[R_EAX]),
                                                       width);
    ok &= state->readCpuRegisterConcrete(CPU_OFFSET(regs[R_EBX]), &messagePtr,
                                         sizeof messagePtr);

    if (!ok) {
        s2e()->getWarningsStream(state)
            << "ERROR: symbolic argument was passed to s2e_kill_state \n";
    } else {
        message="<NO MESSAGE>";
        if(messagePtr && !state->readString(messagePtr, message)) {
            s2e()->getWarningsStream(state)
                << "Error reading message string from the guest\n";
        }
    }

    //Kill the current state
    s2e()->getMessagesStream(state) << "Killing state "  << state->getID() << '\n';
    std::ostringstream os;
    os << "State was terminated by opcode\n"
       << "            message: \"" << message << "\"\n"
       << "            status: " << status;
    s2e()->getExecutor()->terminateStateEarly(*state, os.str());
}

void BaseInstructions::printExpression(S2EExecutionState *state)
{
    //Print the expression
#ifdef TARGET_X86_64
    const klee::Expr::Width width = klee::Expr::Int64;
#else
    const klee::Expr::Width width = klee::Expr::Int32;
#endif

    target_ulong name;
    bool ok = true;
    ref<Expr> val = state->readCpuRegister(offsetof(CPUX86State, regs[R_EAX]),
                                           width);
    ok &= state->readCpuRegisterConcrete(CPU_OFFSET(regs[R_ECX]),
                                         &name, sizeof name);

    if(!ok) {
        s2e()->getWarningsStream(state)
            << "ERROR: symbolic argument was passed to s2e_op "
               "print_expression opcode\n";
        return;
    }

    std::string nameStr = "<NO NAME>";
    if(name && !state->readString(name, nameStr)) {
        s2e()->getWarningsStream(state)
                << "Error reading string from the guest\n";
    }


    s2e()->getMessagesStream() << "SymbExpression " << nameStr << " - "
                               <<val << '\n';

    if (ConcolicMode) {
        klee::ref<klee::Expr> concrete = state->concolics.evaluate(val);
        s2e()->getMessagesStream() << "SymbExpression " << nameStr << " - Value: "
                                   << concrete << '\n';
    }
}

void BaseInstructions::printMemory(S2EExecutionState *state)
{
    target_ulong address, size, name;
    bool ok = true;
    ok &= state->readCpuRegisterConcrete(CPU_OFFSET(regs[R_EAX]),
                                         &address, sizeof address);
    ok &= state->readCpuRegisterConcrete(CPU_OFFSET(regs[R_EBX]),
                                         &size, sizeof size);
    ok &= state->readCpuRegisterConcrete(CPU_OFFSET(regs[R_ECX]),
                                         &name, sizeof name);

    if(!ok) {
        s2e()->getWarningsStream(state)
            << "ERROR: symbolic argument was passed to s2e_op "
               "print_expression opcode\n";
        return;
    }

    std::string nameStr = "<NO NAME>";
    if(name && !state->readString(name, nameStr)) {
        s2e()->getWarningsStream(state)
                << "Error reading string from the guest\n";
    }

    s2e()->getMessagesStream() << "Symbolic memory dump of " << nameStr << '\n';

    for (uint32_t i=0; i<size; ++i) {

        s2e()->getMessagesStream() << hexval(address+i) << ": ";
        ref<Expr> res = state->readMemory8(address+i);
        if (res.isNull()) {
            s2e()->getMessagesStream() << "Invalid pointer\n";
        }else {
            s2e()->getMessagesStream() << res << '\n';
        }
    }
}


void BaseInstructions::concretize(S2EExecutionState *state, bool addConstraint)
{
    target_ulong address, size;

    bool ok = true;
    ok &= state->readCpuRegisterConcrete(CPU_OFFSET(regs[R_EAX]),
                                         &address, sizeof address);
    ok &= state->readCpuRegisterConcrete(CPU_OFFSET(regs[R_EBX]),
                                         &size, sizeof size);

    if(!ok) {
        s2e()->getWarningsStream(state)
            << "ERROR: symbolic argument was passed to s2e_op "
               " get_example opcode\n";
        return;
    }

    for(unsigned i = 0; i < size; ++i) {
        uint8_t b = 0;
        if (!state->readMemoryConcrete8(address + i, &b, S2EExecutionState::VirtualAddress, addConstraint)) {
            s2e()->getWarningsStream(state)
                << "Can not concretize memory"
                << " at " << hexval(address + i) << '\n';
        } else {
            //readMemoryConcrete8 does not automatically overwrite the destination
            //address if we choose not to add the constraint, so we do it here
            if (!addConstraint) {
                if (!state->writeMemoryConcrete(address + i, &b, sizeof(b))) {
                    s2e()->getWarningsStream(state)
                        << "Can not write memory"
                        << " at " << hexval(address + i) << '\n';
                }
            }
        }
    }
}

void BaseInstructions::sleep(S2EExecutionState *state)
{
    target_ulong duration = 0;
    state->readCpuRegisterConcrete(CPU_OFFSET(regs[R_EAX]), &duration,
                                   sizeof duration);
    s2e()->getDebugStream() << "Sleeping " << duration << " seconds\n";

    llvm::sys::TimeValue startTime = llvm::sys::TimeValue::now();

    while (llvm::sys::TimeValue::now().seconds() - startTime.seconds() < duration) {
        #ifdef _WIN32
        Sleep(1000);
        #else
        ::sleep(1);
        #endif
    }
}

void BaseInstructions::printMessage(S2EExecutionState *state, bool isWarning)
{
    target_ulong address = 0;
    bool ok = state->readCpuRegisterConcrete(CPU_OFFSET(regs[R_EAX]),
                                                &address, sizeof address);
    if(!ok) {
        s2e()->getWarningsStream(state)
            << "ERROR: symbolic argument was passed to s2e_op "
               " message opcode\n";
        return;
    }

    std::string str="";
    if(!address || !state->readString(address, str)) {
        s2e()->getWarningsStream(state)
                << "Error reading string message from the guest at address "
                << hexval(address) << '\n';
    } else {
        llvm::raw_ostream *stream;
        if(isWarning)
            stream = &s2e()->getWarningsStream(state);
        else
            stream = &s2e()->getMessagesStream(state);
        (*stream) << "Message from guest (" << hexval(address) <<
                     "): " <<  str << '\n';
    }
}

void BaseInstructions::invokePlugin(S2EExecutionState *state)
{
    BaseInstructionsPluginInvokerInterface *iface = NULL;
    Plugin *plugin;
    std::string pluginName;
    target_ulong pluginNamePointer = 0;
    target_ulong dataPointer = 0;
    target_ulong dataSize = 0;
    target_ulong result = 0;
    bool ok = true;

    ok &= state->readCpuRegisterConcrete(CPU_OFFSET(regs[R_EAX]), &pluginNamePointer, sizeof(pluginNamePointer));
    ok &= state->readCpuRegisterConcrete(CPU_OFFSET(regs[R_ECX]), &dataPointer, sizeof(dataPointer));
    ok &= state->readCpuRegisterConcrete(CPU_OFFSET(regs[R_EDX]), &dataSize, sizeof(dataSize));
    if(!ok) {
        s2e()->getWarningsStream(state)
            << "ERROR: symbolic arguments was passed to s2e_op invokePlugin opcode\n";
        result = 1;
        goto fail;
    }


    if (!state->readString(pluginNamePointer, pluginName)) {
        s2e()->getWarningsStream(state)
            << "ERROR: invokePlugin could not read name of plugin to invoke\n";
        result = 2;
        goto fail;
    }

    plugin = s2e()->getPlugin(pluginName);
    if (!plugin) {
        s2e()->getWarningsStream(state)
            << "ERROR: invokePlugin could not find plugin " << pluginName << "\n";
        result = 3;
        goto fail;
    }

    iface = dynamic_cast<BaseInstructionsPluginInvokerInterface*>(plugin);

    if (!iface) {
        s2e()->getWarningsStream(state)
            << "ERROR: " << pluginName << " is not an instance of BaseInstructionsPluginInvokerInterface\n";
        result = 4;
        goto fail;
    }

    iface->handleOpcodeInvocation(state, dataPointer, dataSize);

 fail:
    state->writeCpuRegisterConcrete(CPU_OFFSET(regs[R_EAX]), &result, sizeof(result));
}

void BaseInstructions::assume(S2EExecutionState *state)
{
    klee::ref<klee::Expr> expr = state->readCpuRegister(CPU_OFFSET(regs[R_EAX]), klee::Expr::Int32);

    klee::ref<klee::Expr> zero = klee::ConstantExpr::create(0, expr.get()->getWidth());
    klee::ref<klee::Expr> boolExpr = klee::NeExpr::create(expr, zero);


    //Check that the added constraint is consistent with
    //the existing path constraints
    bool isValid = true;
    if (ConcolicMode) {
        klee::ref<klee::Expr> ce = state->concolics.evaluate(boolExpr);
        assert(isa<klee::ConstantExpr>(ce) && "Expression must be constant here");
        if (!ce->isTrue()) {
            isValid = false;
        }
    } else {
        bool truth;
        Solver *solver = s2e()->getExecutor()->getSolver();
        Query query(state->constraints, boolExpr);
        bool res = solver->mustBeTrue(query.negateExpr(), truth);
        if (!res || truth) {
            isValid = false;
        }
    }

    if (!isValid) {
        std::stringstream ss;
        ss << "BaseInstructions: specified assume expression cannot be true. "
                << boolExpr;
        g_s2e->getExecutor()->terminateStateEarly(*state, ss.str());
    }

    state->addConstraint(boolExpr);
}

/** Handle s2e_op instruction. Instructions:
    0f 3f XX XX XX XX XX XX XX XX
    XX: opcode
 */
void BaseInstructions::handleBuiltInOps(S2EExecutionState* state, uint64_t opcode)
{
    switch((opcode>>8) & 0xFF) {
        case 0: { /* s2e_check */
                target_ulong v = 1;
                state->writeCpuRegisterConcrete(CPU_OFFSET(regs[R_EAX]), &v,
                                                sizeof v);
            }
            break;
        case 1: state->enableSymbolicExecution(); break;
        case 2: state->disableSymbolicExecution(); break;

        case 3: { /* s2e_make_symbolic */
            makeSymbolic(state, false);
            break;
        }

        case 4: { /* s2e_is_symbolic */
            isSymbolic(state);
            break;
        }

        case 5: { /* s2e_get_path_id */
            const klee::Expr::Width width = sizeof (target_ulong) << 3;
            state->writeCpuRegister(offsetof(CPUX86State, regs[R_EAX]),
                klee::ConstantExpr::create(state->getID(), width));
            break;
        }

        case 6: { /* s2e_kill_state */
            killState(state);
            break;
            }

        case 7: { /* s2e_print_expression */
            printExpression(state);
            break;
        }

        case 8: { //Print memory contents
            printMemory(state);
            break;
        }

        case 9: {
            state->enableForking();
            break;
        }

        case 0xa: {
            state->disableForking();
            break;
        }

        case 0xb: {
            invokePlugin(state);
            break;
        }

        case 0xc: {
            assume(state);
            break;
        }

        case 0xF: { // MJR
            s2e()->getExecutor()->yieldState(*state);
            break;
        }

        case 0x10: { /* s2e_print_message */
            printMessage(state, opcode >> 16);
            break;
        }

        case 0x11: { /* s2e_make_concolic */
            makeSymbolic(state, true);
            break;
        }

        case 0x20: /* concretize */
            concretize(state, true);
            break;

        case 0x21: { /* replace an expression by one concrete example */
            concretize(state, false);
            break;
        }

        case 0x30: { /* Get number of active states */
            target_ulong count = s2e()->getExecutor()->getStatesCount();
            state->writeCpuRegisterConcrete(CPU_OFFSET(regs[R_EAX]), &count,
                                            sizeof(count));
            break;
        }

        case 0x31: { /* Get number of active S2E instances */
            target_ulong count = s2e()->getCurrentProcessCount();
            state->writeCpuRegisterConcrete(CPU_OFFSET(regs[R_EAX]), &count,
                                            sizeof(count));
            break;
        }
        case 0x32: { /* Sleep for a given number of seconds */
           sleep(state);
           break;
        }

        case 0x50: { /* disable/enable timer interrupt */
            uint64_t disabled = opcode >> 16;
            if(disabled)
                s2e()->getMessagesStream(state) << "Disabling timer interrupt\n";
            else
                s2e()->getMessagesStream(state) << "Enabling timer interrupt\n";
            state->writeCpuState(CPU_OFFSET(timer_interrupt_disabled),
                                 disabled, 8);
            break;
        }
        case 0x51: { /* disable/enable all apic interrupts */
            uint64_t disabled = opcode >> 16;
            if(disabled)
                s2e()->getMessagesStream(state) << "Disabling all apic interrupt\n";
            else
                s2e()->getMessagesStream(state) << "Enabling all apic interrupt\n";
            state->writeCpuState(CPU_OFFSET(all_apic_interrupts_disabled),
                                 disabled, 8);
            break;
        }

        case 0x52: { /* Gets the current S2E memory object size (in power of 2) */
                target_ulong size = S2E_RAM_OBJECT_BITS;
                state->writeCpuRegisterConcrete(CPU_OFFSET(regs[R_EAX]), &size,
                                                sizeof size);
                break;
        }

        case 0x70: /* merge point */
            state->jumpToSymbolicCpp();
            s2e()->getExecutor()->queueStateForMerge(state);
            break;

        default:
            s2e()->getWarningsStream(state)
                << "BaseInstructions: Invalid built-in opcode " << hexval(opcode) << '\n';
            break;
    }
}

void BaseInstructions::onCustomInstruction(S2EExecutionState* state, 
        uint64_t opcode)
{
    uint8_t opc = (opcode>>8) & 0xFF;
    if (opc <= 0x70) {
        handleBuiltInOps(state, opcode);
    }
}

}
}
