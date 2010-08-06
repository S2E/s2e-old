extern "C" {
#include "config.h"
#include "qemu-common.h"
}

#include "FunctionMonitor.h"
#include <s2e/S2E.h>
#include <s2e/ConfigFile.h>
#include <s2e/Utils.h>

#include <iostream>

namespace s2e {
namespace plugins {

S2E_DEFINE_PLUGIN(FunctionMonitor, "Function calls/returns monitoring plugin", "",);

void FunctionMonitor::initialize()
{
    s2e()->getCorePlugin()->onTranslateBlockEnd.connect(
            sigc::mem_fun(*this, &FunctionMonitor::slotTranslateBlockEnd));

    s2e()->getCorePlugin()->onTranslateJumpStart.connect(
            sigc::mem_fun(*this, &FunctionMonitor::slotTranslateJumpStart));

    if(s2e()->getConfig()->getBool(getConfigKey() + ".enableTracing")) {
        getCallSignal(0, 0)->connect(sigc::mem_fun(*this,
                                     &FunctionMonitor::slotTraceCall));
    }

    m_monitor = static_cast<OSMonitor*>(s2e()->getPlugin("Interceptor"));
}

FunctionMonitor::CallSignal* FunctionMonitor::getCallSignal(
        S2EExecutionState *state,
        uint64_t eip, uint64_t cr3)
{
    DECLARE_PLUGINSTATE(FunctionMonitorState, state);

    return plgState->getCallSignal(eip, cr3);
}

void FunctionMonitor::slotTranslateBlockEnd(ExecutionSignal *signal,
                                      S2EExecutionState *state,
                                      TranslationBlock *tb,
                                      uint64_t pc, bool, uint64_t)
{
    /* We intercept all call and ret translation blocks */
    if (tb->s2e_tb_type == TB_CALL || tb->s2e_tb_type == TB_CALL_IND) {
        signal->connect(sigc::mem_fun(*this,
                            &FunctionMonitor::slotCall));
    }
}

void FunctionMonitor::slotTranslateJumpStart(ExecutionSignal *signal,
                                             S2EExecutionState *state,
                                             TranslationBlock *,
                                             uint64_t, int jump_type)
{
    if(jump_type == JT_RET || jump_type == JT_LRET) {
        signal->connect(sigc::mem_fun(*this,
                            &FunctionMonitor::slotRet));
    }
}

void FunctionMonitor::slotCall(S2EExecutionState *state, uint64_t pc)
{
    DECLARE_PLUGINSTATE(FunctionMonitorState, state);

    return plgState->slotCall(state, pc);
}

//See notes for slotRet to see how to use this function.
void FunctionMonitor::eraseSp(S2EExecutionState *state, uint64_t pc)
{
    DECLARE_PLUGINSTATE(FunctionMonitorState, state);

    return plgState->slotRet(state, pc, false);
}



void FunctionMonitor::slotRet(S2EExecutionState *state, uint64_t pc)
{
    DECLARE_PLUGINSTATE(FunctionMonitorState, state);

    return plgState->slotRet(state, pc, true);
}


void FunctionMonitor::slotTraceCall(S2EExecutionState *state, ReturnSignal *signal)
{
    static int f = 0;
    signal->connect(sigc::bind(sigc::mem_fun(*this, &FunctionMonitor::slotTraceRet), f));
    s2e()->getMessagesStream(state) << "Calling function " << f
                << " at " << hexval(state->getPc()) << std::endl;
    ++f;
}

void FunctionMonitor::slotTraceRet(S2EExecutionState *state, int f)
{
    s2e()->getMessagesStream(state) << "Returning from function "
                << f << std::endl;
}


FunctionMonitorState::FunctionMonitorState()
{

}

FunctionMonitorState::~FunctionMonitorState()
{

}

FunctionMonitorState* FunctionMonitorState::clone() const
{
    return new FunctionMonitorState(*this);
}

PluginState *FunctionMonitorState::factory(Plugin *p, S2EExecutionState *s)
{
    FunctionMonitorState *ret = new FunctionMonitorState();
    ret->m_plugin = static_cast<FunctionMonitor*>(p);
    return ret;
}

FunctionMonitor::CallSignal* FunctionMonitorState::getCallSignal(
        uint64_t eip, uint64_t cr3)
{
    std::pair<CallDescriptorsMap::iterator, CallDescriptorsMap::iterator>
            range = m_callDescriptors.equal_range(eip);

    for(CallDescriptorsMap::iterator it = range.first; it != range.second; ++it) {
        if(it->second.cr3 == cr3)
            return &it->second.signal;
    }

    CallDescriptor descriptor = { cr3, FunctionMonitor::CallSignal() };
    CallDescriptorsMap::iterator it =
            m_newCallDescriptors.insert(std::make_pair(eip, descriptor));
    return &it->second.signal;
}


void FunctionMonitorState::slotCall(S2EExecutionState *state, uint64_t pc)
{
    target_ulong cr3 = state->getPid();
    target_ulong eip = state->getPc();
    target_ulong esp;
    bool ok = state->readCpuRegisterConcrete(CPU_OFFSET(regs[R_ESP]),
                                             &esp, sizeof(target_ulong));
    if(!ok) {
        m_plugin->s2e()->getWarningsStream(state)
            << "Function call with symbolic ESP!" << std::endl
            << "  EIP=" << hexval(eip) << " CR3=" << hexval(cr3) << std::endl;
        return;
    }

    if (!m_newCallDescriptors.empty()) {
        m_callDescriptors.insert(m_newCallDescriptors.begin(), m_newCallDescriptors.end());
        m_newCallDescriptors.clear();
    }

    /* Issue signals attached to all calls (eip==-1 means catch-all) */
    if (!m_callDescriptors.empty()) {
        std::pair<CallDescriptorsMap::iterator, CallDescriptorsMap::iterator>
                range = m_callDescriptors.equal_range((uint64_t)-1);
        for(CallDescriptorsMap::iterator it = range.first; it != range.second; ++it) {
            CallDescriptor cd = (*it).second;
            if (m_plugin->m_monitor) {
                cr3 = m_plugin->m_monitor->getPid(state, pc);
            }
            if(it->second.cr3 == (uint64_t)-1 || it->second.cr3 == cr3) {
                ReturnDescriptor descriptor = {cr3, FunctionMonitor::ReturnSignal() };
                cd.signal.emit(state, &descriptor.signal);
                if(!descriptor.signal.empty()) {
                    m_returnDescriptors.insert(std::make_pair(esp, descriptor));
                }
            }
        }
        if (!m_newCallDescriptors.empty()) {
            m_callDescriptors.insert(m_newCallDescriptors.begin(), m_newCallDescriptors.end());
            m_newCallDescriptors.clear();
        }
    }

    /* Issue signals attached to specific calls */
    if (!m_callDescriptors.empty()) {
        std::pair<CallDescriptorsMap::iterator, CallDescriptorsMap::iterator>
                range;

        range = m_callDescriptors.equal_range(eip);
        for(CallDescriptorsMap::iterator it = range.first; it != range.second; ++it) {
            CallDescriptor cd = (*it).second;
            if (m_plugin->m_monitor) {
                cr3 = m_plugin->m_monitor->getPid(state, pc);
            }
            if(it->second.cr3 == (uint64_t)-1 || it->second.cr3 == cr3) {
                ReturnDescriptor descriptor = { it->second.cr3 , FunctionMonitor::ReturnSignal() };
                cd.signal.emit(state, &descriptor.signal);
                if(!descriptor.signal.empty()) {
                    m_returnDescriptors.insert(std::make_pair(esp, descriptor));
                }
            }
        }
        if (!m_newCallDescriptors.empty()) {
            m_callDescriptors.insert(m_newCallDescriptors.begin(), m_newCallDescriptors.end());
            m_newCallDescriptors.clear();
        }
    }
}

/**
 *  When emitSignal is false, this function simply removes all the return descriptors
 * for the current stack pointer. This can be used when a return handler manually changes the
 * program counter and/or wants to exit to the cpu loop and avoid being called again.
 *
 *  Note: all the return handlers will be erased if emitSignal is false, not just the one
 * that issued the call. Also note that it not possible to return from the handler normally
 * whenever this function is called from within a return handler.
 */
void FunctionMonitorState::slotRet(S2EExecutionState *state, uint64_t pc, bool emitSignal)
{
    target_ulong cr3 = state->readCpuState(CPU_OFFSET(cr[3]), 8*sizeof(target_ulong));

    target_ulong esp;
    bool ok = state->readCpuRegisterConcrete(CPU_OFFSET(regs[R_ESP]),
                                             &esp, sizeof(target_ulong));
    if(!ok) {
        target_ulong eip = state->readCpuState(CPU_OFFSET(eip),
                                               8*sizeof(target_ulong));
        m_plugin->s2e()->getWarningsStream(state)
            << "Function return with symbolic ESP!" << std::endl
            << "  EIP=" << hexval(eip) << " CR3=" << hexval(cr3) << std::endl;
        return;
    }

    if (m_returnDescriptors.empty()) {
        return;
    }

    bool finished = true;
    do {
        finished = true;
        std::pair<ReturnDescriptorsMap::iterator, ReturnDescriptorsMap::iterator>
                range = m_returnDescriptors.equal_range(esp);
        for(ReturnDescriptorsMap::iterator it = range.first; it != range.second; ++it) {
            if (m_plugin->m_monitor) {
                cr3 = m_plugin->m_monitor->getPid(state, pc);
            }

            if(it->second.cr3 == cr3) {
                if (emitSignal) {
                    it->second.signal.emit(state);
                }
                m_returnDescriptors.erase(it);
                finished = false;
                break;
            }
        }
    } while(!finished);
}


} // namespace plugins
} // namespace s2e
