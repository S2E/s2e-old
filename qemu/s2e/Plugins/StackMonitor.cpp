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

#include "StackMonitor.h"
#include <s2e/S2E.h>
#include <s2e/ConfigFile.h>
#include <s2e/Utils.h>
#include <s2e/s2e_qemu.h>

#include <iostream>

namespace s2e {
namespace plugins {

S2E_DEFINE_PLUGIN(StackMonitor, "Tracks stack usage by modules", "StackMonitor",
                  "ModuleExecutionDetector", "Interceptor");

class StackMonitorState : public PluginState
{
public:
    class ModuleCache {
    private:
        typedef std::pair<uint64_t, uint64_t> PidPc;
        typedef std::map<PidPc, unsigned> Cache;
        Cache m_cache;
        unsigned m_lastId;

    public:
        ModuleCache() {
            m_lastId = 0;
        }

        void addModule(const ModuleDescriptor &module) {
            PidPc p = std::make_pair(module.Pid, module.LoadBase);
            if (m_cache.find(p) != m_cache.end()) {
                return;
            }

            unsigned id = ++m_lastId;
            m_cache[p] = id;
        }

        void removeModule(const ModuleDescriptor &module) {
            PidPc p = std::make_pair(module.Pid, module.LoadBase);
            m_cache.erase(p);
        }

        unsigned getId(const ModuleDescriptor &module) const {
            PidPc p = std::make_pair(module.Pid, module.LoadBase);
            Cache::const_iterator it = m_cache.find(p);
            if (it == m_cache.end()) {
                return 0;
            }

            return (*it).second;
        }
    };

    struct StackFrame {
        uint64_t pc; //Program counter that opened the frame
        uint64_t top;
        uint64_t size;
        unsigned moduleId; //Index in a cache to avoid duplication

        bool operator < (const StackFrame &f1) {
            return top + size <= f1.size;
        }

        friend llvm::raw_ostream& operator<<(llvm::raw_ostream &os, const StackFrame &frame);
    };

    //The frames are sorted by decreasing stack pointer
    typedef std::vector<StackFrame> StackFrames;


    class Stack {
        uint64_t m_stackBase;
        uint64_t m_stackSize;

        //XXX: remove it?
        uint64_t m_lastStackPointer;

        StackFrames m_frames;

    public:
        Stack(S2EExecutionState *state,
              StackMonitorState *plgState,
              uint64_t pc,
              uint64_t base, uint64_t size) {

            m_stackBase = base;
            m_stackSize = size;
            m_lastStackPointer = state->getSp();

            const ModuleDescriptor *module = plgState->m_detector->getModule(state, pc);
            assert(module && "BUG: StackMonitor should only track configured modules");

            StackFrame sf;
            sf.moduleId = plgState->m_moduleCache.getId(*module);
            assert(sf.moduleId && "BUG: StackMonitor did not register a tracked module");

            sf.top = m_lastStackPointer;
            sf.size = 4; //XXX: Fix constant
            sf.pc = pc;

            m_frames.push_back(sf);
        }

        uint64_t getStackBase() const {
            return m_stackBase;
        }

        uint64_t getStackSize() const {
            return m_stackSize;
        }

        /** Used for call instructions */
        void newFrame(S2EExecutionState *state, unsigned currentModuleId, uint64_t pc, uint64_t stackPointer) {
            const StackFrame &last = m_frames.back();
            assert(stackPointer < last.top + last.size);

            StackFrame frame;
            frame.pc = pc;
            frame.moduleId = currentModuleId;
            frame.top = stackPointer;
            frame.size = 4;
            m_frames.push_back(frame);

            m_lastStackPointer = stackPointer;
        }

        void update(S2EExecutionState *state, unsigned currentModuleId, uint64_t stackPointer) {
            assert(!m_frames.empty());
            assert(stackPointer >= m_stackBase && stackPointer < (m_stackBase + m_stackSize));
            StackFrame &last = m_frames.back();

            //The current stack pointer is above the bottom of the stack
            //We need to unwind the frames
            do {
                if (last.top >= stackPointer) {
                    last.size = last.top - stackPointer + 4;
                } else {
                    m_frames.pop_back();
                }

                if (m_frames.empty()) {
                    break;
                }

                last = m_frames.back();
            } while (stackPointer > last.top);

            // The stack may become empty when the last frame is popped,
            // e.g., when the top-level function returns.
        }

        /** Check whether there is a frame that belongs to the module. */
        bool hasModule(unsigned moduleId) {
            foreach2(it, m_frames.begin(), m_frames.end()) {
                if ((*it).moduleId == moduleId) {
                    return true;
                }
            }
            return false;
        }

        bool removeAllFrames(unsigned moduleId) {
            StackFrames::iterator it = m_frames.begin();

            unsigned i=0;
            while (i < m_frames.size()) {
                if (m_frames[i].moduleId == moduleId) {
                    m_frames.erase(it + i);
                } else {
                    ++i;
                }
            }
            return m_frames.empty();
        }

        bool empty() const {
            return m_frames.empty();
        }

        bool getFrame(uint64_t sp, bool &frameValid, StackFrame &frameInfo) const {
            if (sp < m_stackBase  || (sp >= m_stackBase + m_stackSize)) {
                return false;
            }

            frameValid = false;

            //Look for the right frame
            //XXX: Use binary search?
            foreach2(it, m_frames.begin(), m_frames.end()) {
                const StackFrame &frame= *it;
                if (sp > frame.top || (sp < frame.top - frame.size)) {
                    continue;
                }

                frameValid = true;
                frameInfo = frame;
                break;
            }

            return true;
        }

        void getCallStack(CallStack &cs) const {
            foreach2(it, m_frames.begin(), m_frames.end()) {
                cs.push_back((*it).pc);
            }
        }

        friend llvm::raw_ostream& operator<<(llvm::raw_ostream &os, const Stack &stack);
    };

    //Maps a stack base to a stack representation
    typedef std::pair<uint64_t, uint64_t> PidStackBase;
    typedef std::map<PidStackBase, Stack> Stacks;
private:
    bool m_debugMessages;
    uint64_t m_pid;
    uint64_t m_cachedStackBase;
    uint64_t m_cachedStackSize;
    OSMonitor *m_monitor;
    ModuleExecutionDetector *m_detector;
    StackMonitor *m_stackMonitor;
    Stacks m_stacks;
    ModuleCache m_moduleCache;

public:

    void update(S2EExecutionState *state, uint64_t pc, bool isCall);
    void onModuleUnload(S2EExecutionState* state, const ModuleDescriptor &module);
    void onModuleLoad(S2EExecutionState* state, const ModuleDescriptor &module);
    void deleteStack(S2EExecutionState *state, uint64_t stackBase);

    bool getFrameInfo(S2EExecutionState *state, uint64_t sp, bool &onTheStack, StackFrameInfo &info) const;
    bool getCallStacks(S2EExecutionState *state, CallStacks &callStacks) const;

    void dump(S2EExecutionState *state) const;
public:
    StackMonitorState(bool debugMessages);
    virtual ~StackMonitorState();
    virtual StackMonitorState* clone() const;
    static PluginState *factory(Plugin *p, S2EExecutionState *s);

    friend class StackMonitor;
};

llvm::raw_ostream& operator<<(llvm::raw_ostream &os, const StackMonitorState::StackFrame &frame)
{
    os << "  Frame pc=" << hexval(frame.pc) << " @" << hexval(frame.top) << " size=" << hexval(frame.size);
    return os;
}

llvm::raw_ostream& operator<<(llvm::raw_ostream &os, const StackMonitorState::Stack &stack)
{
    os << "Stack " << hexval(stack.m_stackBase) << " size=" << hexval(stack.m_stackSize) << "\n";
    foreach2(it, stack.m_frames.begin(), stack.m_frames.end()) {
        os << *it << "\n";
    }

    return os;
}


void StackMonitor::initialize()
{
    m_detector = static_cast<ModuleExecutionDetector*>(s2e()->getPlugin("ModuleExecutionDetector"));
    m_monitor = static_cast<OSMonitor*>(s2e()->getPlugin("Interceptor"));
    m_statsCollector = static_cast<ExecutionStatisticsCollector*>(s2e()->getPlugin("ExecutionStatisticsCollector"));

    m_debugMessages = s2e()->getConfig()->getBool(getConfigKey() + ".debugMessages");

    //m_monitor->onThreadCreate.connect(
    //    sigc::mem_fun(*this, &StackMonitor::onThreadCreate));

    m_monitor->onThreadExit.connect(
        sigc::mem_fun(*this, &StackMonitor::onThreadExit));

    m_detector->onModuleTranslateBlockStart.connect(
                sigc::mem_fun(*this, &StackMonitor::onModuleTranslateBlockStart));

    m_detector->onModuleTranslateBlockEnd.connect(
                sigc::mem_fun(*this, &StackMonitor::onModuleTranslateBlockEnd));

    m_monitor->onModuleLoad.connect(
                sigc::mem_fun(*this, &StackMonitor::onModuleLoad));

    m_monitor->onModuleUnload.connect(
                sigc::mem_fun(*this, &StackMonitor::onModuleUnload));

    m_detector->onModuleTransition.connect(
                sigc::mem_fun(*this, &StackMonitor::onModuleTransition));
}

void StackMonitor::onModuleTranslateBlockStart(ExecutionSignal *signal,
        S2EExecutionState *state, const ModuleDescriptor &desc,
        TranslationBlock *tb, uint64_t pc)
{
    m_onTranslateRegisterAccessConnection.disconnect();

    m_onTranslateRegisterAccessConnection =
            s2e()->getCorePlugin()->onTranslateRegisterAccessEnd.connect(
                sigc::mem_fun(*this, &StackMonitor::onTranslateRegisterAccess));
}

void StackMonitor::onModuleTranslateBlockEnd(
        ExecutionSignal *signal, S2EExecutionState* state,
        const ModuleDescriptor &desc, TranslationBlock *tb,
        uint64_t endPc, bool staticTarget, uint64_t targetPc)
{
    m_onTranslateRegisterAccessConnection.disconnect();
}

void StackMonitor::onTranslateRegisterAccess(
             ExecutionSignal *signal, S2EExecutionState* state, TranslationBlock* tb,
             uint64_t pc, uint64_t rmask, uint64_t wmask, bool accessesMemory)
{
    if ((wmask & (1 << R_ESP))) {
        bool isCall = false;
        if (tb->s2e_tb_type == TB_CALL || tb->s2e_tb_type == TB_CALL_IND) {
            isCall = true;
        }

        signal->connect(sigc::bind(sigc::mem_fun(*this, &StackMonitor::onStackPointerModification), isCall));
    }
}

void StackMonitor::onStackPointerModification(S2EExecutionState *state, uint64_t pc, bool isCall)
{
    //s2e()->getDebugStream() << "StackMonitor: ESP modif at " << hexval(pc) << "\n";
    DECLARE_PLUGINSTATE(StackMonitorState, state);
    plgState->update(state, pc, isCall);
}

void StackMonitor::onThreadCreate(S2EExecutionState *state, const ThreadDescriptor &thread)
{
    //s2e()->getDebugStream() << "StackMonitor: ThreadCreate StackBase=" << hexval(thread.KernelStackBottom)
    //                        << " StackSize=" << hexval(thread.KernelStackSize) << "\n";
}

void StackMonitor::onThreadExit(S2EExecutionState *state, const ThreadDescriptor &thread)
{
    //s2e()->getDebugStream() << "StackMonitor: ThreadExit StackBase=" << hexval(thread.KernelStackBottom)
    //                        << " StackSize=" << hexval(thread.KernelStackSize) << "\n";
    DECLARE_PLUGINSTATE(StackMonitorState, state);
    assert(thread.KernelMode && "How do we deal with user-mode stacks?");

    plgState->deleteStack(state, thread.KernelStackBottom);
}

void StackMonitor::onModuleLoad(S2EExecutionState* state, const ModuleDescriptor &module)
{
    if (!m_detector->getModuleId(module)) {
        return;
    }

    DECLARE_PLUGINSTATE(StackMonitorState, state);
    plgState->onModuleLoad(state, module);
}


void StackMonitor::onModuleUnload(S2EExecutionState* state, const ModuleDescriptor &module)
{
    DECLARE_PLUGINSTATE(StackMonitorState, state);
    plgState->onModuleUnload(state, module);
}

void StackMonitor::onModuleTransition(S2EExecutionState* state, const ModuleDescriptor *prev,
                                      const ModuleDescriptor *next)
{
    if (next == NULL) {
        return;
    }

    DECLARE_PLUGINSTATE(StackMonitorState, state);
    plgState->update(state, state->getPc(), false);
}

bool StackMonitor::getFrameInfo(S2EExecutionState *state, uint64_t sp, bool &onTheStack, StackFrameInfo &info) const
{
    DECLARE_PLUGINSTATE(StackMonitorState, state);
    return plgState->getFrameInfo(state, sp, onTheStack, info);
}

void StackMonitor::dump(S2EExecutionState *state)
{
    //s2e()->getDebugStream() << "StackMonitor: ESP modif at " << hexval(pc) << "\n";
    DECLARE_PLUGINSTATE(StackMonitorState, state);
    plgState->dump(state);
}

bool StackMonitor::getCallStacks(S2EExecutionState *state, CallStacks &callStacks) const
{
    DECLARE_PLUGINSTATE(StackMonitorState, state);
    return plgState->getCallStacks(state, callStacks);
}

/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/

StackMonitorState::StackMonitorState(bool debugMessages)
{
    m_cachedStackBase = 0;
    m_cachedStackSize = 0;
    m_pid = 0;
    m_debugMessages = debugMessages;
    m_monitor = static_cast<OSMonitor*>(g_s2e->getPlugin("Interceptor"));
    m_detector = static_cast<ModuleExecutionDetector*>(g_s2e->getPlugin("ModuleExecutionDetector"));
    m_stackMonitor = static_cast<StackMonitor*>(g_s2e->getPlugin("StackMonitor"));
}

StackMonitorState::~StackMonitorState()
{

}

StackMonitorState* StackMonitorState::clone() const
{
    return new StackMonitorState(*this);
}

PluginState *StackMonitorState::factory(Plugin *p, S2EExecutionState *s)
{
    StackMonitor *sm = static_cast<StackMonitor*>(g_s2e->getPlugin("StackMonitor"));
    return new StackMonitorState(sm->m_debugMessages);
}


void StackMonitorState::update(S2EExecutionState *state, uint64_t pc, bool isCall)
{
    uint64_t sp = state->getSp();
    uint64_t pid = m_monitor->getPid(state, pc);

    if ((pid != m_pid) || !(sp >= m_cachedStackBase && sp < (m_cachedStackBase + m_cachedStackSize))) {
        m_pid = pid;
        if (!m_monitor->getCurrentStack(state, &m_cachedStackBase, &m_cachedStackSize)) {
            g_s2e->getWarningsStream() << "StackMonitor: could not get current stack\n";
            return;
        }
    }

    PidStackBase p = std::make_pair(pid, m_cachedStackBase);

    Stacks::iterator stackit = m_stacks.find(p);
    if (stackit == m_stacks.end()) {
        Stack stack(state, this, pc, m_cachedStackBase, m_cachedStackSize);
        m_stacks.insert(std::make_pair(p, stack));
        stackit = m_stacks.find(p);
        m_stackMonitor->onStackCreation.emit(state);
    }

    const ModuleDescriptor *module = m_detector->getModule(state, pc);
    assert(module && "BUG: unknown module");

    unsigned moduleId = m_moduleCache.getId(*module);

    Stack &stack = (*stackit).second;

    if (isCall) {
        stack.newFrame(state, moduleId, pc, sp);
    } else {
        (*stackit).second.update(state, moduleId, sp);
    }

    if ( m_debugMessages) {
        g_s2e->getDebugStream() << (*stackit).second << "\n";
    }

    if (stack.empty()) {
        m_stacks.erase(stackit);
        m_stackMonitor->onStackDeletion.emit(state);
        if (m_stackMonitor->m_statsCollector) {
            m_stackMonitor->m_statsCollector->incrementEmptyCallStacksCount(state);
        }
    }
}

void StackMonitorState::onModuleLoad(S2EExecutionState* state, const ModuleDescriptor &module)
{
    m_moduleCache.addModule(module);
}

void StackMonitorState::onModuleUnload(S2EExecutionState* state, const ModuleDescriptor &module)
{
    unsigned id = m_moduleCache.getId(module);

    //The stack monitor has never seen the module executed/accessing the stack
    if (!id) {
        return;
    }

    //Remove all frames that belong to the module.
    //If there are any frames active, this usually means a bug as a module cannot
    //be usually unloaded unless all its methods finished executing.
    //XXX: Leave this check to bug checkers (put an event here).
    Stacks::iterator it = m_stacks.begin();
    while (it != m_stacks.end()) {
        if ((*it).second.removeAllFrames(id)) {
            //The stack is empty, get rid of it
            m_stacks.erase(it++);
        } else {
            ++it;
        }
    }
}

void StackMonitorState::deleteStack(S2EExecutionState *state, uint64_t stackBase)
{
    PidStackBase p = std::make_pair(m_monitor->getPid(state, state->getPc()), stackBase);
    Stacks::iterator it = m_stacks.find(p);
    if (it == m_stacks.end()) {
        return;
    }

    m_stacks.erase(it);
}

//onTheStack == true && result == true ==> found a valid frame
//onTheStack == true && result == false ==> on the stack but not in any know frame
//onTheStack == false ==> does not fall in any know stack
bool StackMonitorState::getFrameInfo(S2EExecutionState *state, uint64_t sp, bool &onTheStack, StackFrameInfo &info) const
{
    uint64_t pid = m_monitor->getPid(state, state->getPc());
    onTheStack = false;

    //XXX: Assume here that there are very few stacks, so simple iteration is fast enough
    foreach2(it, m_stacks.begin(), m_stacks.end()) {
        if ((*it).first.first != pid) {
            continue;
        }

        const Stack &stack = (*it).second;
        StackFrame frameInfo;
        bool frameValid = false;
        if (!stack.getFrame(sp, frameValid, frameInfo)) {
            continue;
        }

        onTheStack = true;

        if (frameValid) {
            info.FramePc = frameInfo.pc;
            info.FrameSize = frameInfo.size;
            info.FrameTop = frameInfo.top;
            info.StackBase = stack.getStackBase();
            info.StackSize = stack.getStackSize();
            return true;
        }

        return false;
    }

    return false;
}

void StackMonitorState::dump(S2EExecutionState *state) const
{
    g_s2e->getDebugStream() << "Dumping stacks\n";
    foreach2(it, m_stacks.begin(), m_stacks.end()) {
        g_s2e->getDebugStream() << (*it).second << "\n";
    }
}

bool StackMonitorState::getCallStacks(S2EExecutionState *state, CallStacks &callStacks) const
{
    foreach2(it, m_stacks.begin(), m_stacks.end()) {
        callStacks.push_back(CallStack());
        CallStack &cs = callStacks.back();

        const Stack &stack = (*it).second;
        stack.getCallStack(cs);
    }
    return true;
}

} // namespace plugins
} // namespace s2e
