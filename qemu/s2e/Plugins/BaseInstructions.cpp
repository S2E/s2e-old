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

#include <llvm/System/TimeValue.h>
#include <klee/Searcher.h>

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

bool BaseInstructions::createTables()
{
    const char *query = "create table TimingInstructions(" 
          "'timestamp' unsigned big int,"  
          "'data' unsigned big int,"
          "'pc' unsigned big int,"
          "'pid' unsigned big int"
          "); create index if not exists TimingInstructionsIdx on TimingInstructions(timestamp);";
    
    Database *db = s2e()->getDb();
    return db->executeQuery(query);
}

bool BaseInstructions::insertTiming(S2EExecutionState *state, uint64_t id)
{
    char buf[512];
    uint64_t timestamp = llvm::sys::TimeValue::now().usec();
    snprintf(buf, sizeof(buf), "insert into TimingInstructions('%"PRIu64"', '%"PRIu64"', '%"PRIu64"', '%"PRIu64"');", 
        timestamp, id, state->getPc(), state->getPid());
    
    Database *db = s2e()->getDb();
    return db->executeQuery(buf);
}

/** Handle s2e_op instruction. Instructions:
    0f 3f 00 01 - enable symbolic execution
    0f 3f 00 02 - disable symbolic execution
    0f 3f 00 03 - insert symbolic value at address pointed by eax
                  with the size in ebx and name (asciiz) in ecx
    0f 3f 00 04 - kill the current state
    0f 3f 00 10 00 - print message (asciiz) pointed by eax
    0f 3f 00 10 01 - print warning (asciiz) pointed by eax
 */
void BaseInstructions::handleBuiltInOps(S2EExecutionState* state, uint64_t opcode)
{
    switch((opcode>>8) & 0xFF) {
        case 1: s2e()->getExecutor()->enableSymbolicExecution(state); break;
        case 2: s2e()->getExecutor()->disableSymbolicExecution(state); break;
        case 3: { /* make_symbolic */
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
                       " insert_symbolic opcode" << std::endl;
                break;
            }

            std::string nameStr;
            if(!state->readString(name, nameStr)) {
                s2e()->getWarningsStream(state)
                        << "Error reading string from the guest"
                        << std::endl;
                break;
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
        case 4: 
            {
                //Timing opcode
                uint64_t low = state->readCpuState(offsetof(CPUX86State, regs[R_EAX]), sizeof(uint32_t));
                uint64_t high = state->readCpuState(offsetof(CPUX86State, regs[R_EDX]), sizeof(uint32_t));
                insertTiming(state, (high << 32LL) | low);
                break;
            }

        case 5:
            {
                //Get current path
                state->writeCpuRegister(offsetof(CPUX86State, regs[R_EAX]),
                    klee::ConstantExpr::create(123, klee::Expr::Int32));
                break;
            }

        case 6:
            {
                //Kill the current state
                s2e()->getMessagesStream(state) << "Killing state "  << state->getID() << std::endl;
                s2e()->getExecutor()->terminateStateOnExit(*state);
                break;
            }

        case 0x10: { /* print message */
            uint32_t address; //XXX
            bool ok = state->readCpuRegisterConcrete(CPU_OFFSET(regs[R_EAX]),
                                                        &address, 4);
            if(!ok) {
                s2e()->getWarningsStream(state)
                    << "ERROR: symbolic argument was passed to s2e_op "
                       " message opcode" << std::endl;
                break;
            }

            std::string str;
            if(!state->readString(address, str)) {
                s2e()->getWarningsStream(state)
                        << "Error reading string message from the guest"
                        << std::endl;
            } else {
                ostream *stream;
                if(opcode >> 16)
                    stream = &s2e()->getWarningsStream(state);
                else
                    stream = &s2e()->getMessagesStream(state);
                (*stream) << "Message from guest: " << str << std::endl;
            }
            break;
        }
        default:
            s2e()->getWarningsStream(state)
                << "Invalid built-in opcode " << hexval(opcode) << std::endl;
            break;
    }
}

void BaseInstructions::onCustomInstruction(S2EExecutionState* state, 
        uint64_t opcode)
{
    TRACE("Custom instruction %#"PRIx64"\n", opcode);

    switch(opcode & 0xFF) {
        case 0x00:
            handleBuiltInOps(state, opcode);
            break;
        default:
            std::cout << "Invalid custom operation 0x"<< std::hex << opcode<< " at 0x" << 
                state->getPc() << std::endl;
    }
}

}
}
