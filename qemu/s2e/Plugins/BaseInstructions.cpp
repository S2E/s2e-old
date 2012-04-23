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

extern "C" {
#include "config.h"
#include "qemu-common.h"
}

#ifdef CONFIG_WIN32
#include <windows.h>
#endif

#include "BaseInstructions.h"
#include <s2e/S2E.h>
#include <s2e/Database.h>
#include <s2e/S2EExecutor.h>
#include <s2e/S2EExecutionState.h>
#include <s2e/ConfigFile.h>
#include <s2e/Utils.h>

#include <iostream>
#include <sstream>

#include <llvm/Support/TimeValue.h>
#include <klee/Searcher.h>
#include <klee/Solver.h>

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
    uint32_t address, size, name; // XXX
    bool ok = true;
    ok &= state->readCpuRegisterConcrete(CPU_OFFSET(regs[R_EAX]),
                                         &address, 4);
    ok &= state->readCpuRegisterConcrete(CPU_OFFSET(regs[R_EBX]),
                                         &size, 4);
    ok &= state->readCpuRegisterConcrete(CPU_OFFSET(regs[R_ECX]),
                                         &name, 4);

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
    uint32_t address;
    uint32_t result;
    char buf;
    bool ok = true;
    ok &= state->readCpuRegisterConcrete(CPU_OFFSET(regs[R_ECX]),
                                         &address, 4);

    if(!ok) {
        s2e()->getWarningsStream(state)
            << "ERROR: symbolic argument was passed to s2e_op is_symbolic\n";
        return;
    }

    s2e()->getMessagesStream(state)
            << "Testing whether data at " << hexval(address)
            << " is symbolic:";

    // readMemoryConcrete fails if the value is symbolic
    result = !state->readMemoryConcrete(address, &buf, 1);
    s2e()->getMessagesStream(state)
            << (result ? " true" : " false") << '\n';
    state->writeCpuRegisterConcrete(CPU_OFFSET(regs[R_EAX]), &result, 4);
}

void BaseInstructions::killState(S2EExecutionState *state)
{
    std::string message;
    uint32_t messagePtr;
    bool ok = true;
    klee::ref<klee::Expr> status = state->readCpuRegister(CPU_OFFSET(regs[R_EAX]), klee::Expr::Int32);
    ok &= state->readCpuRegisterConcrete(CPU_OFFSET(regs[R_EBX]), &messagePtr, 4);

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
    uint32_t name; //xxx
    bool ok = true;
    ref<Expr> val = state->readCpuRegister(offsetof(CPUX86State, regs[R_EAX]), klee::Expr::Int32);
    ok &= state->readCpuRegisterConcrete(CPU_OFFSET(regs[R_ECX]),
                                         &name, 4);

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
}

void BaseInstructions::printMemory(S2EExecutionState *state)
{
    uint32_t address, size, name; // XXX should account for 64 bits archs
    bool ok = true;
    ok &= state->readCpuRegisterConcrete(CPU_OFFSET(regs[R_EAX]),
                                         &address, 4);
    ok &= state->readCpuRegisterConcrete(CPU_OFFSET(regs[R_EBX]),
                                         &size, 4);
    ok &= state->readCpuRegisterConcrete(CPU_OFFSET(regs[R_ECX]),
                                         &name, 4);

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
    uint32_t address, size;

    bool ok = true;
    ok &= state->readCpuRegisterConcrete(CPU_OFFSET(regs[R_EAX]),
                                         &address, 4);
    ok &= state->readCpuRegisterConcrete(CPU_OFFSET(regs[R_EBX]),
                                         &size, 4);

    if(!ok) {
        s2e()->getWarningsStream(state)
            << "ERROR: symbolic argument was passed to s2e_op "
               " get_example opcode\n";
        return;
    }

    for(unsigned i = 0; i < size; ++i) {
        if (!state->readMemoryConcrete8(address + i, NULL, S2EExecutionState::VirtualAddress, addConstraint)) {
            s2e()->getWarningsStream(state)
                << "Can not concretize memory"
                << " at " << hexval(address + i) << '\n';
        }
    }
}

void BaseInstructions::sleep(S2EExecutionState *state)
{
    uint32_t duration = 0;
    state->readCpuRegisterConcrete(CPU_OFFSET(regs[R_EAX]), &duration, sizeof(uint32_t));
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
    uint32_t address = 0; //XXX
    bool ok = state->readCpuRegisterConcrete(CPU_OFFSET(regs[R_EAX]),
                                                &address, 4);
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

/** Handle s2e_op instruction. Instructions:
    0f 3f XX XX XX XX XX XX XX XX
    XX: opcode
 */
void BaseInstructions::handleBuiltInOps(S2EExecutionState* state, uint64_t opcode)
{
    switch((opcode>>8) & 0xFF) {
        case 0: { /* s2e_check */
                uint32_t v = 1;
                state->writeCpuRegisterConcrete(CPU_OFFSET(regs[R_EAX]), &v, 4);
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
            state->writeCpuRegister(offsetof(CPUX86State, regs[R_EAX]),
                klee::ConstantExpr::create(state->getID(), klee::Expr::Int32));
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

        case 9:
            state->enableForking();
            break;

        case 10:
            state->disableForking();
            break;

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
            uint32_t count = s2e()->getExecutor()->getStatesCount();
            state->writeCpuRegisterConcrete(CPU_OFFSET(regs[R_EAX]), &count, sizeof(uint32_t));
            break;
        }

        case 0x31: { /* Get number of active S2E instances */
            uint32_t count = s2e()->getCurrentProcessCount();
            state->writeCpuRegisterConcrete(CPU_OFFSET(regs[R_EAX]), &count, sizeof(uint32_t));
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
                uint32_t size = S2E_RAM_OBJECT_BITS;
                state->writeCpuRegisterConcrete(CPU_OFFSET(regs[R_EAX]), &size, 4);
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
