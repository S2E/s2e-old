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

#include <llvm/Support/TimeValue.h>

#include <s2e/ConfigFile.h>
#include <s2e/Utils.h>
#include <s2e/s2e_qemu.h>

#include "StateManager.h"
#include <klee/Searcher.h>

#ifdef CONFIG_WIN32
#include <windows.h>
#endif

namespace s2e {
namespace plugins {

S2E_DEFINE_PLUGIN(StateManager, "Control the deletion/suspension of states", "StateManager",
                  "ModuleExecutionDetector");

void sm_callback(S2EExecutionState *s, bool killingState)
{
    StateManager *sm = static_cast<StateManager*>(g_s2e->getPlugin("StateManager"));
    assert(sm);

    if (killingState && s) {
        sm->resumeSucceededState(s);
        return;
    }

    sm->m_shared.acquire();
    sm->checkInvariants();

    //Process the queued commands for the current process
    sm->processCommands();

    //If there are no states, try to resume some successful ones
    if (g_s2e->getExecutor()->getStatesCount() == 0) {
        g_s2e->getDebugStream() << "No more active states" << '\n';

        //If there are no successful states on the local process,
        //there is nothing else to do, kill the process
        if (sm->m_succeeded.size() == 0) {
            g_s2e->getDebugStream() << "No more succeeded states" << '\n';
            sm->m_shared.release();
            return;
        }

        sm->m_shared.release();

        sm->suspendCurrentProcess();

        //Only a kill all can resume us, so we must process commands now
        sm->m_shared.acquire();
        sm->processCommands();
        sm->m_shared.release();
        return;
    }

    //Check for timeout conditions
    sm->killOnTimeOut();
    sm->m_shared.release();
}

//XXX: Assumes we are called from the callback
void StateManager::suspendCurrentProcess()
{
    s2e()->getDebugStream() << "Suspending process" << '\n';
    unsigned currentProcessId = s2e()->getCurrentProcessId();

    StateManagerShared *shared = m_shared.acquire();
    shared->suspendedProcesses[currentProcessId] = true;
    m_shared.release();

    while(true) {
        shared = m_shared.tryAcquire();
        if (shared) {
            //Somebody woke us up
            if (!shared->suspendedProcesses[currentProcessId]) {
                m_shared.release();
                return;
            }

            //There are no more active processes in the system,
            if (getSuspendedProcessCount() == s2e()->getCurrentProcessCount()) {
                resumeAllProcesses();
                killAllButOneSuccessful();
                m_shared.release();
                return;
            }
            m_shared.release();
        }
        #ifdef CONFIG_WIN32
        Sleep(1000);
        #else
        sleep(1);
        #endif
    }
}

void StateManager::resumeAllProcesses()
{
    s2e()->getDebugStream() << "Resuming all processes" << '\n';
    StateManagerShared *shared = m_shared.get();

    unsigned maxProcessCount = s2e()->getMaxProcesses();
    for (unsigned i=0; i<maxProcessCount; ++i) {
        shared->suspendedProcesses[i] = false;
    }
}


unsigned StateManager::getSuspendedProcessCount()
{
    StateManagerShared *shared = m_shared.get();
    unsigned count = 0;
    //Process may come and go and scattered across the array
    //so use max processes instead of the current count
    unsigned maxProcessCount = s2e()->getMaxProcesses();
    for (unsigned i=0; i<maxProcessCount; ++i) {
        if (shared->suspendedProcesses[i]) {
            ++count;
        }
    }
    return count;
}


void StateManager::checkInvariants(bool grabLock)
{
    if (grabLock) {
        m_shared.acquire();
    }

    uint64_t *successCount = m_shared.get()->successCount;
    if (successCount[s2e()->getCurrentProcessId()] != m_succeeded.size()) {
        unsigned procId = s2e()->getCurrentProcessId();
        s2e()->getWarningsStream() << "successCount[" << procId << "]=" << successCount[procId] << '\n';
        s2e()->getWarningsStream() << "m_succeeded.size()=" << m_succeeded.size() << '\n';
        assert(successCount[procId] == m_succeeded.size());
    }

    if (grabLock) {
        m_shared.release();
    }
}

void StateManager::sendKillToAllInstances(bool keepOneSuccessful, unsigned procId)
{
    StateManagerShared *s = m_shared.get();

    StateManagerShared::Command cmd;
    cmd.command = StateManagerShared::KILL;

    //Write a command to each instance, which will eventually execute it
    unsigned maxProcessCount = s2e()->getMaxProcesses();
    for(unsigned i=0; i<maxProcessCount; ++i) {
        if (i != s2e()->getCurrentProcessId()) {
            cmd.nodeId = keepOneSuccessful ? procId : (uint8_t)-1;
            s->commands[i].write(cmd);
        }
    }
}

bool StateManager::processCommands()
{
    StateManagerShared *s = m_shared.get();


    StateManagerShared::Command cmd = s->commands[s2e()->getCurrentProcessId()].read();

    if (cmd.command == StateManagerShared::KILL) {
        cmd.command = StateManagerShared::EMPTY;
        s->commands[s2e()->getCurrentProcessId()].write(cmd);
        s2e()->getDebugStream() << "StateManager: received kill command" << '\n';
        if (cmd.nodeId == s2e()->getCurrentProcessId()) {
            //Keep one successful
            assert(s->keepOneStateOnNode == cmd.nodeId);
            //It may happen that wake up kills all states locally. Skip this case here.
            s->keepOneStateOnNode = (unsigned)-1;
            if (m_succeeded.size() > 0) {
                killAllButOneSuccessfulLocal(true);
            }
        }else {
            //Kill everything
            StateSet toKeep;
            resumeSucceeded();
            killAllExcept(toKeep, true);
        }
    }

    return true;
}


bool StateManager::timeoutReached() const
{
    if (!m_timeout) {
        return false;
    }

    uint64_t prevTime = (int64_t)AtomicFunctions::read(&m_shared.get()->timeOfLastNewBlock);

    if (prevTime > m_currentTime) {
        //Other nodes may be ahead of the current one in terms of time
        return false;
    }

    return m_currentTime - prevTime >= m_timeout;
}

void StateManager::resetTimeout()
{
    llvm::sys::TimeValue curTime = llvm::sys::TimeValue::now();
    AtomicFunctions::write(&m_shared.get()->timeOfLastNewBlock, curTime.seconds());
    m_currentTime = curTime.seconds();
}

void StateManager::resumeSucceeded()
{
    checkInvariants();
    uint64_t *successCount = m_shared.get()->successCount;

    foreach2(it, m_succeeded.begin(), m_succeeded.end()) {
        m_executor->resumeState(*it);
    }
    m_succeeded.clear();

    successCount[s2e()->getCurrentProcessId()] = 0;
}

bool StateManager::resumeSucceededState(S2EExecutionState *s)
{
    if (m_succeeded.find(s) != m_succeeded.end()) {
        uint64_t *successCount = m_shared.get()->successCount;

        checkInvariants();
        --successCount[s2e()->getCurrentProcessId()];

        m_succeeded.erase(s);
        m_executor->resumeState(s);
        return true;
    }
    return false;
}

StateManager::~StateManager()
{
    StateManagerShared *shared = m_shared.acquire();
    uint64_t *successCount = shared->successCount;

    checkInvariants();
    unsigned procId = s2e()->getCurrentProcessId();
    successCount[procId] -= m_succeeded.size();
    if (shared->keepOneStateOnNode == procId) {
        shared->keepOneStateOnNode = (unsigned)-1;
    }

    m_shared.release();
}

void StateManager::initialize()
{
    ConfigFile *cfg = s2e()->getConfig();

    m_timeout = cfg->getInt(getConfigKey() + ".timeout");
    resetTimeout();

    m_detector = static_cast<ModuleExecutionDetector*>(s2e()->getPlugin("ModuleExecutionDetector"));

    m_detector->onModuleTranslateBlockStart.connect(
            sigc::mem_fun(*this,
                    &StateManager::onNewBlockCovered)
            );

    s2e()->getCorePlugin()->onProcessFork.connect(
            sigc::mem_fun(*this,
                    &StateManager::onProcessFork)
            );

    s2e()->getCorePlugin()->onCustomInstruction.connect(
            sigc::mem_fun(*this, &StateManager::onCustomInstruction));

    s2e()->getCorePlugin()->onTimer.connect(
            sigc::mem_fun(*this, &StateManager::onTimer));

    m_executor = s2e()->getExecutor();
    m_executor->setStateManagerCb(sm_callback);
}

void StateManager::onTimer()
{
    //Calling this is very expensive and should be done as rarely as possible.
    llvm::sys::TimeValue curTime = llvm::sys::TimeValue::now();
    m_currentTime = curTime.seconds();
}

void StateManager::onProcessFork(bool preFork, bool isChild, unsigned parentProcId)
{
    if (preFork) {
        checkInvariants(true);
        return;
    }

    if (isChild) {
        unsigned procId = s2e()->getCurrentProcessId();

        s2e()->getDebugStream() << "StateManager forked curProc=" << procId <<
                " parentProcId=" << parentProcId << '\n';

        StateManagerShared *s = m_shared.acquire();
        s->successCount[procId] = m_succeeded.size();
        StateManagerShared::Command cmd = {0,0,0,0};
        s->commands[procId].write(cmd);
        s->suspendedProcesses[procId] = false;
        m_shared.release();
    }

    checkInvariants(true);
}

//Reset the timeout every time a new block of the module is translated.
//XXX: this is an approximation. The cache could be flushed in between.
//XXX: don't reset the timeout if this block has been covered somewhere else.
void StateManager::onNewBlockCovered(
        ExecutionSignal *signal,
        S2EExecutionState* state,
        const ModuleDescriptor &module,
        TranslationBlock *tb,
        uint64_t pc)
{
    s2e()->getDebugStream() << "New block " << hexval(pc) << " discovered" << '\n';
    resetTimeout();
}

bool StateManager::killOnTimeOut()
{
    if (!timeoutReached()) {
        return false;
    }

    s2e()->getDebugStream() << "No more blocks found in "
            << m_timeout << " seconds, killing states."
            << '\n';

    //Reset the counter here to avoid being called again
    //(killAllButOneSuccessful will throw an exception if it deletes the current state).
    resetTimeout();

    if (!killAllButOneSuccessful()) {
        s2e()->getDebugStream() << "There are no successful states to kill..."  << '\n';
        return false;
    }
    return true;
}


bool StateManager::killAllExcept(StateSet &toKeep, bool ungrab)
{
    llvm::raw_ostream &os = s2e()->getDebugStream();
    os << "StateManager: killAllExcept ";
    foreach2(it, toKeep.begin(), toKeep.end()) {
        os << (*it)->getID() << " ";
    }
    os << '\n';

    bool killCurrent = false;
    const std::set<klee::ExecutionState*> &states = s2e()->getExecutor()->getStates();
    std::set<klee::ExecutionState*>::const_iterator it = states.begin();

    while(it != states.end()) {
        S2EExecutionState *curState = static_cast<S2EExecutionState*>(*it);
        if (toKeep.find(curState) != toKeep.end()) {
            ++it;
            continue;
        }

        ++it;
        if (curState == g_s2e_state) {
            killCurrent = true;
        }else {
            s2e()->getExecutor()->terminateStateEarly(*curState, "StateManager: killing state");
        }
    }

    //In case we need to kill the current state, do it last, because it will throw and exception
    //and return to the state scheduler.
    if (killCurrent) {
        if (ungrab) {
            m_shared.release();
        }
        s2e()->getExecutor()->terminateStateEarly(*g_s2e_state, "StateManager: killing state");
    }

    return true;
}

void StateManager::killAllButOneSuccessfulLocal(bool ungrabLock)
{
    s2e()->getDebugStream() << "StateManager: killAllButOneSuccessfulLocal" << '\n';
    checkInvariants();
    assert(m_succeeded.size() > 0);
    S2EExecutionState *one =  *m_succeeded.begin();
    resumeSucceeded();

    StateSet toKeep;
    toKeep.insert(one);

    killAllExcept(toKeep, ungrabLock);
}

bool StateManager::killAllButOneSuccessful()
{
    StateManagerShared *shared = m_shared.get();
    uint64_t *successCount = shared->successCount;
    checkInvariants();

    unsigned maxProcesses = s2e()->getMaxProcesses();

    //Determine the instance that has at least one successful state
    unsigned hasSuccessfulIndex = shared->keepOneStateOnNode;
    if ((hasSuccessfulIndex == (unsigned)-1) || (m_succeeded.size() == 0)) {
        for (hasSuccessfulIndex=0; hasSuccessfulIndex < maxProcesses; ++hasSuccessfulIndex) {
            if (s2e()->getProcessIndexForId(hasSuccessfulIndex) == (unsigned)-1) {
                continue;
            }
            if (successCount[hasSuccessfulIndex] > 0) {
                break;
            }
        }
    }

    assert(hasSuccessfulIndex != (unsigned)-1);

    //There are no successful states anywhere, just return
    if (hasSuccessfulIndex == maxProcesses) {
        return false;
    }

    s2e()->getDebugStream() << "StateManager: Killing all but one successful on node "
            << s2e()->getProcessIndexForId(hasSuccessfulIndex) << '\n';

    //Kill all states everywhere except one successful on the instance that we found
    if (hasSuccessfulIndex == s2e()->getCurrentProcessId()) {
        //We chose one state on our local instance
        assert(successCount[hasSuccessfulIndex] == m_succeeded.size());

        //Ask other instances to kill all their states
        sendKillToAllInstances(false, 0);

        //Kill all local states
        killAllButOneSuccessfulLocal(true);
    }else {
        //All concurrent kills will have to chose the same node
        //to avoid killing everything by accident
        shared->keepOneStateOnNode = hasSuccessfulIndex;

        //We chose a state on a different instance
        sendKillToAllInstances(true, hasSuccessfulIndex);

        //Kill everything locally
        StateSet toKeep;
        killAllExcept(toKeep, true);
    }

    return true;
}

/*********************************************************************************************/
/*********************************************************************************************/
/*********************************************************************************************/

bool StateManager::succeedState(S2EExecutionState *s)
{
    s2e()->getDebugStream() << "Succeeding state " << s->getID() << '\n';
    checkInvariants();

    if (m_succeeded.find(s) != m_succeeded.end()) {
        //Do not suspend states that were consecutively succeeded.
        s2e()->getDebugStream() << "State " << s->getID() <<
                " was already marked as succeeded" << '\n';
        return false;
    }
    m_succeeded.insert(s);

    bool ret =  s2e()->getExecutor()->suspendState(s);

    StateManagerShared *shared = m_shared.acquire();
    shared->successCount[s2e()->getCurrentProcessId()] = m_succeeded.size();
    m_shared.release();

    return ret;
}

bool StateManager::empty()
{
    assert(s2e()->getExecutor()->getSearcher());
    return s2e()->getExecutor()->getSearcher()->empty();
}

//Allows to control behavior directly from guest code
void StateManager::onCustomInstruction(S2EExecutionState* state, uint64_t opcode)
{
    if (!OPCODE_CHECK(opcode, STATE_MANAGER_OPCODE)) {
        return;
    }

    unsigned subfunc = OPCODE_GETSUBFUNCTION(opcode);
    switch (subfunc) {
        case SUCCEED: {
            succeedState(state);

            //At this point, we must advance the program counter, otherwise
            //the current custom instruction will be executed again when the state resumes.
            target_ulong pc = state->getPc() + OPCODE_SIZE;
            state->writeCpuState(CPU_OFFSET(eip), pc, 8*sizeof(target_ulong));

            throw CpuExitException();
            break;
        }

        //Count the number of successful states across all nodes
        case GET_SUCCESSFUL_STATE_COUNT: {
            StateManagerShared *s = m_shared.acquire();
            uint32_t count=0;
            for (unsigned i=0;i<s2e()->getMaxProcesses(); ++i) {
                count += s->successCount[i];
            }
            m_shared.release();

            state->writeCpuRegisterConcrete(CPU_OFFSET(regs[R_EAX]), &count, sizeof(uint32_t));
        }

        default:
            s2e()->getWarningsStream() << "StateManager: incorrect opcode " << hexval(subfunc) << '\n';
    }
}

}
}
