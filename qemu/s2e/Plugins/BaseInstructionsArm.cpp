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
 *    Andreas Kirchner <a0600112@unet.univie.ac.at>
 *
 * All contributors are listed in S2E-AUTHORS file.
 *
 */

extern "C" {
#include "config.h"
#include "qemu-common.h"
}


#include "BaseInstructions.h"
#include <s2e/S2E.h>
#include <s2e/Database.h>
#include <s2e/S2EExecutor.h>
#include <s2e/S2EExecutionState.h>
#include <s2e/ConfigFile.h>
#include <s2e/Utils.h>

#include <iostream>
#include <sstream>

#include <llvm/System/TimeValue.h>
#include <klee/Searcher.h>
#include <klee/Solver.h>

namespace s2e {
namespace plugins {

using namespace std;
using namespace klee;

S2E_DEFINE_PLUGIN(BaseInstructions, "Default set of custom instructions plugin for ARM", "",);

void BaseInstructions::initialize()
{
    s2e()->getCorePlugin()->onCustomInstruction.connect(
            sigc::mem_fun(*this, &BaseInstructions::onCustomInstruction));

}

/** Handle s2e_op instruction. Instructions:
    ff XX XX XX
    XX: opcode
 */
void BaseInstructions::handleBuiltInOps(S2EExecutionState* state, uint64_t opcode)
{

	s2e()->getWarningsStream(state)
	                        << "handleBuildInOps with opcode "
	                        << opcode
	                        << " called."
	                        << std::endl;

    switch((opcode>>16) & 0xFF) {
        case 0: { /* s2e_check */
                uint32_t v = 2;
                state->writeCpuRegisterConcrete(CPU_OFFSET(regs[0]), &v, 4);
            }
            break;
        case 1: state->enableSymbolicExecution(); break;
        case 2: state->disableSymbolicExecution(); break;

        case 3: { /* make_symbolic */
            uint32_t address, size, name; // XXX
            bool ok = true;
            ok &= state->readCpuRegisterConcrete(CPU_OFFSET(regs[0]),
                                                 &address, 4);
            ok &= state->readCpuRegisterConcrete(CPU_OFFSET(regs[1]),
                                                 &size, 4);
            ok &= state->readCpuRegisterConcrete(CPU_OFFSET(regs[2]),
                                                 &name, 4);

            if(!ok) {
                s2e()->getWarningsStream(state)
                    << "ERROR: symbolic argument was passed to s2e_op "
                       " insert_symbolic opcode" << std::endl;
                break;
            }

            std::string nameStr;
            if(!name || !state->readString(name, nameStr)) {
                s2e()->getWarningsStream(state)
                        << "Error reading string from the guest"
                        << std::endl;
                nameStr = "defstr";
            }

            s2e()->getMessagesStream(state)
                    << "Inserting symbolic data at " << hexval(address)
                    << " of size " << hexval(size)
                    << " with name '" << nameStr << "'" << std::endl;

            vector<ref<Expr> > symb = state->createSymbolicArray(size, nameStr);
            for(unsigned i = 0; i < size; ++i) {
                if(!state->writeMemory8(address + i, symb[i])) {
                    s2e()->getWarningsStream(state)
                        << "Can not insert symbolic value"
                        << " at " << hexval(address + i)
                        << ": can not write to memory" << std::endl;
                }
            }
            break;
        }

        case 5:
            {
                //Get current path
                state->writeCpuRegister(offsetof(CPUARMState, regs[0]),
                    klee::ConstantExpr::create(state->getID(), klee::Expr::Int32));
                break;
            }

        case 6:
            {
                std::string message;
                uint32_t messagePtr;
                bool ok = true;
                klee::ref<klee::Expr> status = state->readCpuRegister(CPU_OFFSET(regs[0]), klee::Expr::Int32);
                ok &= state->readCpuRegisterConcrete(CPU_OFFSET(regs[1]), &messagePtr, 4);

                if (!ok) {
                    s2e()->getWarningsStream(state)
                        << "ERROR: symbolic argument was passed to s2e_op kill state "
                        << std::endl;
                } else {
                    message="<NO MESSAGE>";
                    if(messagePtr && !state->readString(messagePtr, message)) {
                        s2e()->getWarningsStream(state)
                            << "Error reading file name string from the guest" << std::endl;
                    }
                }

                //Kill the current state
                s2e()->getMessagesStream(state) << "Killing state "  << state->getID() << std::endl;
                std::ostringstream os;
                os << "State was terminated by opcode\n"
                   << "            message: \"" << message << "\"\n"
                   << "            status: " << status;
                s2e()->getExecutor()->terminateStateEarly(*state, os.str());
                break;
            }

        case 7:
            {
                //Print the expression
                uint32_t name; //xxx
                bool ok = true;
                ref<Expr> val = state->readCpuRegister(offsetof(CPUARMState, regs[0]), klee::Expr::Int32);
                ok &= state->readCpuRegisterConcrete(CPU_OFFSET(regs[1]),
                                                     &name, 4);

                if(!ok) {
                    s2e()->getWarningsStream(state)
                        << "ERROR: symbolic argument was passed to s2e_op "
                           "print_expression opcode" << std::endl;
                    break;
                }

                std::string nameStr = "defstring";
                if(!name || !state->readString(name, nameStr)) {
                    s2e()->getWarningsStream(state)
                            << "Error reading string from the guest"
                            << std::endl;
                }


                s2e()->getMessagesStream() << "SymbExpression " << nameStr << " - "
                        <<val << std::endl;
                break;
            }

        case 8:
            {
                //Print memory contents
                uint32_t address, size, name; // XXX should account for 64 bits archs
                bool ok = true;
                ok &= state->readCpuRegisterConcrete(CPU_OFFSET(regs[0]),
                                                     &address, 4);
                ok &= state->readCpuRegisterConcrete(CPU_OFFSET(regs[1]),
                                                     &size, 4);
                ok &= state->readCpuRegisterConcrete(CPU_OFFSET(regs[2]),
                                                     &name, 4);

                if(!ok) {
                    s2e()->getWarningsStream(state)
                        << "ERROR: symbolic argument was passed to s2e_op "
                           "print_expression opcode" << std::endl;
                    break;
                }

                std::string nameStr = "defstring";
                if(!name || !state->readString(name, nameStr)) {
                    s2e()->getWarningsStream(state)
                            << "Error reading string from the guest"
                            << std::endl;
                }

                s2e()->getMessagesStream() << "Symbolic memory dump of " << nameStr << std::endl;

                for (uint32_t i=0; i<size; ++i) {

                    s2e()->getMessagesStream() << std::hex << "0x" << std::setw(8) << (address+i) << ": " << std::dec;
                    ref<Expr> res = state->readMemory8(address+i);
                    if (res.isNull()) {
                        s2e()->getMessagesStream() << "Invalid pointer" << std::endl;
                    }else {
                        s2e()->getMessagesStream() << res << std::endl;
                    }
                }

                break;
            }

        case 9: state->enableForking(); break;
        case 10: state->disableForking(); break;


        case 0x10: { /* print message */
            uint32_t address; //XXX
            bool ok = state->readCpuRegisterConcrete(CPU_OFFSET(regs[0]),
                                                        &address, 4);
            if(!ok) {
                s2e()->getWarningsStream(state)
                    << "ERROR: symbolic argument was passed to s2e_op "
                       " message opcode" << std::endl;
                break;
            }

            std::string str="";
            if(!state->readString(address, str)) {
                s2e()->getWarningsStream(state)
                        << "Error reading string message from the guest at address 0x"
                        << std::hex << address
                        << std::endl;
            } else {
                ostream *stream;
                if(opcode >> 16)
                    stream = &s2e()->getWarningsStream(state);
                else
                    stream = &s2e()->getMessagesStream(state);
                (*stream) << "Message from guest (0x" << std::hex << address <<
                        "): " <<  str << std::endl;
            }
            break;
        }

        case 0x20: /* concretize */
        case 0x21: { /* replace an expression by one concrete example */
            uint32_t address, size;

            bool ok = true;
            ok &= state->readCpuRegisterConcrete(CPU_OFFSET(regs[0]),
                                                 &address, 4);
            ok &= state->readCpuRegisterConcrete(CPU_OFFSET(regs[1]),
                                                 &size, 4);

            if(!ok) {
                s2e()->getWarningsStream(state)
                    << "ERROR: symbolic argument was passed to s2e_op "
                       " get_example opcode" << std::endl;
                break;
            }

            for(unsigned i = 0; i < size; ++i) {
                ref<Expr> expr = state->readMemory8(address + i);
                if(!expr.isNull()) {
                    if(((opcode>>16) & 0xFF) == 0x20) /* concretize */
                        expr = s2e()->getExecutor()->toConstant(*state, expr, "request from guest");
                    else /* example */
                        expr = s2e()->getExecutor()->toConstantSilent(*state, expr);
                    if(!state->writeMemory(address + i, expr)) {
                        s2e()->getWarningsStream(state)
                            << "Can not write to memory"
                            << " at " << hexval(address + i) << std::endl;
                    }
                } else {
                    s2e()->getWarningsStream(state)
                        << "Can not read from memory"
                        << " at " << hexval(address + i) << std::endl;
                }
            }

            break;
        }

        case 0x52: { /* Gets the current S2E memory object size (in power of 2) */
                uint32_t size = S2E_RAM_OBJECT_BITS;
                state->writeCpuRegisterConcrete(CPU_OFFSET(regs[0]), &size, 4);
                break;
        }

        case 0x70: /* merge point */
            s2e()->getExecutor()->jumpToSymbolicCpp(state);
            s2e()->getExecutor()->queueStateForMerge(state);
            break;

    default:
            s2e()->getWarningsStream(state)
                << "BaseInstructions: Invalid built-in opcode " << hexval(opcode) << std::endl;
            break;
    }
}

void BaseInstructions::onCustomInstruction(S2EExecutionState* state,
        uint64_t opcode)
{

	s2e()->getWarningsStream(state)
	                        << "onCustomInstruction with opcode "
	                        << opcode
	                        << " called."
	                        << std::endl;

    uint8_t opc = (opcode>>16) & 0xFF;
    if (opc <= 0x70) {
        handleBuiltInOps(state, opcode);
    }
}

}
}
