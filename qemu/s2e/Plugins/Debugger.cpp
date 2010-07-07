extern "C" {
#include <qemu-common.h>
#include <cpu-all.h>
#include <exec-all.h>
}

#include "Debugger.h"
#include <s2e/S2E.h>
#include <s2e/S2EExecutionState.h>
#include <s2e/ConfigFile.h>
#include <s2e/Utils.h>

#include <iostream>

namespace s2e {
namespace plugins {

S2E_DEFINE_PLUGIN(Debugger, "Debugger plugin", "",);

void Debugger::initialize()
{
    m_dataTriggers = NULL;
    m_dataTriggerCount = 0;

    m_addressTriggers = NULL;
    m_addressTriggerCount = 0;

    //Catch all accesses to the stack
    m_monitorStack = s2e()->getConfig()->getBool(getConfigKey() + ".monitorStack");

    //Catch accesses that are above the specified address
    m_catchAbove = s2e()->getConfig()->getInt(getConfigKey() + ".catchAccessesAbove");

    //Start monitoring after the specified number of seconds
    m_timeTrigger = s2e()->getConfig()->getInt(getConfigKey() + ".timeTrigger");
    m_elapsedTics = 0;

    //Manual addresses
    //XXX: Note that stack monitoring and manual addresses cannot be used together...
    initList(getConfigKey() + ".dataTriggers", &m_dataTriggers, &m_dataTriggerCount);
    initList(getConfigKey() + ".addressTriggers", &m_addressTriggers, &m_addressTriggerCount);

    if (!m_timeTrigger) {
        s2e()->getCorePlugin()->onDataMemoryAccess.connect(
                sigc::mem_fun(*this, &Debugger::onDataMemoryAccess));
    }else {
        m_timerConnection = s2e()->getCorePlugin()->onTimer.connect(
                sigc::mem_fun(*this, &Debugger::onTimer));
    }

}

void Debugger::initList(const std::string &key, uint64_t **ptr, unsigned *size)
{
    ConfigFile::integer_list list;
    list = s2e()->getConfig()->getIntegerList(key);

    *size = list.size();

    if (list.size() > 0) {
        *ptr = new uint64_t[list.size()];
        ConfigFile::integer_list::iterator it;
        unsigned i=0;

        for (it = list.begin(); it != list.end(); ++it) {
            s2e()->getMessagesStream() << "Adding trigger for value 0x" << std::hex << *it << std::endl;
            (*ptr)[i] = *it;
            ++i;
        }
    }
}

Debugger::~Debugger(void)
{
    if (m_dataTriggers) {
        delete [] m_dataTriggers;
    }

    if (m_addressTriggers) {
        delete [] m_addressTriggers;
    }
}

bool Debugger::dataTriggered(uint64_t data) const
{
    for (unsigned i=0; i<m_dataTriggerCount; ++i) {
        if (m_dataTriggers[i] == data) {
            return true;
        }
    }
    return false;
}

bool Debugger::addressTriggered(uint64_t address) const
{
    for (unsigned i=0; i<m_addressTriggerCount; ++i) {
        if (m_addressTriggers[i] == address) {
            return true;
        }
    }
    return false;
}

bool Debugger::decideTracing(S2EExecutionState *state, uint64_t addr, uint64_t data) const
{
    if (m_monitorStack) {
        //Assume that the stack is 8k and 8k-aligned
        if ((state->getSp() & ~0x3FFFF) == (addr & ~0x3FFFF)) {
            return true;
        }
    }

    if (dataTriggered(data)) {
        return true;
    }

    if (addressTriggered(addr)) {
        return true;
    }

    return false;
}

void Debugger::onDataMemoryAccess(S2EExecutionState *state,
                               klee::ref<klee::Expr> address,
                               klee::ref<klee::Expr> hostAddress,
                               klee::ref<klee::Expr> value,
                               bool isWrite, bool isIO)
{
    if(!isa<klee::ConstantExpr>(address) || !isa<klee::ConstantExpr>(value)) {
        //We do not support symbolic values yet...
        return;
    }

    uint64_t addr = cast<klee::ConstantExpr>(address)->getZExtValue(64);
    uint64_t val = cast<klee::ConstantExpr>(value)->getZExtValue(64);

    if (addr < m_catchAbove) {
        //Skip uninteresting ranges
        return;
    }


    if (decideTracing(state, addr, val)) {
        s2e()->getDebugStream() <<
                   " MEM PC=0x" << std::hex << state->getPc() <<
                   " Addr=0x" << addr <<
                   " Value=0x" << val <<
                   " IsWrite=" << isWrite << std::endl;
    }

}

void Debugger::onTranslateInstructionStart(
    ExecutionSignal *signal,
    S2EExecutionState *state,
    TranslationBlock *tb,
    uint64_t pc
    )
{
    signal->connect(sigc::mem_fun(*this, &Debugger::onInstruction));

    /*if (pc <= 0xc02144b1 && pc >= 0xc02142f8) {
        signal->connect(sigc::mem_fun(*this, &Debugger::onInstruction));
    }

    if (pc <= 0xc0204e20 && pc >= 0xc0202e20) {
        signal->connect(sigc::mem_fun(*this, &Debugger::onInstruction));
    }*/

}

void Debugger::onInstruction(S2EExecutionState *state, uint64_t pc)
{
    s2e()->getDebugStream() << std::hex << "IT " << pc <<
            " CC_SRC=" << state->readCpuRegister(offsetof(CPUState, cc_src), klee::Expr::Int32) <<
            std::endl;
}

void Debugger::onTimer()
{
    if (m_elapsedTics++ < m_timeTrigger) {
        return;
    }

    s2e()->getMessagesStream() << "Debugger Plugin: Enabling memory tracing" << std::endl;
/*    s2e()->getCorePlugin()->onDataMemoryAccess.connect(
            sigc::mem_fun(*this, &Debugger::onDataMemoryAccess));*/

    //s2e()->getCorePlugin()->onTranslateInstructionStart.connect(
      //      sigc::mem_fun(*this, &Debugger::onTranslateInstructionStart));

    s2e()->getCorePlugin()->onTranslateInstructionEnd.connect(
            sigc::mem_fun(*this, &Debugger::onTranslateInstructionStart));

    m_timerConnection.disconnect();
}

} // namespace plugins
} // namespace s2e
