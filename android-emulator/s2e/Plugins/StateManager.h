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

#ifndef S2E_STATEMANAGER_H

#define S2E_STATEMANAGER_H

#include <s2e/s2e_config.h>
#include <s2e/Plugin.h>
#include <s2e/Plugins/CorePlugin.h>
#include <s2e/Plugins/ModuleExecutionDetector.h>
#include <s2e/Plugins/Opcodes.h>
#include <s2e/S2EExecutionState.h>
#include <s2e/S2E.h>
#include <s2e/S2EExecutor.h>
#include <set>

namespace s2e {
namespace plugins {

struct StateManagerShared {
    enum Commands {
        EMPTY=0, KILL
    };

    struct Command {
        uint8_t command;
        uint8_t nodeId;
        uint16_t padding;
        uint32_t padding2;
    };

    uint64_t suspendAll;
    uint64_t timeOfLastNewBlock;

    //How many states succeeded in each instance.
    //Access using the current state id modulo max number of processes.
    uint64_t successCount[S2E_MAX_PROCESSES];
    AtomicObject<Command>  commands[S2E_MAX_PROCESSES];
    bool suspendedProcesses[S2E_MAX_PROCESSES];

    //If killing is in progress, indicate which node
    //will keep a successful state. Used to handle concurrent killAlls.
    //-1 if no kill is in progress
    unsigned keepOneStateOnNode;

    StateManagerShared() {
        suspendAll = 0;
        timeOfLastNewBlock = 0;
        Command cmd = {0,0,0,0};
        keepOneStateOnNode = (unsigned)-1;

        for (unsigned i=0; i<S2E_MAX_PROCESSES; ++i) {
            successCount[i] = 0;
            commands[i].write(cmd);
            suspendedProcesses[i] = false;
        }
    }
};

void sm_callback(S2EExecutionState *s, bool killingState);

class StateManager : public Plugin
{
    S2E_PLUGIN
public:
    StateManager(S2E* s2e): Plugin(s2e) {}
    virtual ~StateManager();
    typedef std::set<S2EExecutionState*> StateSet;

    void initialize();

private:

    enum Opcodes {
        SUCCEED=0,
        GET_SUCCESSFUL_STATE_COUNT=1
    };

    StateSet m_succeeded;
    S2EExecutor *m_executor;
    unsigned m_timeout;
    uint64_t m_currentTime;

    ModuleExecutionDetector *m_detector;

    static StateManager *s_stateManager;

    S2ESynchronizedObject<StateManagerShared> m_shared;

    void onNewBlockCovered(
            ExecutionSignal *signal,
            S2EExecutionState* state,
            const ModuleDescriptor &module,
            TranslationBlock *tb,
            uint64_t pc);

    void onProcessFork(bool preFork, bool isChild, unsigned parentProcId);
    void onTimer();

    bool processCommands();


    bool killAllExcept(StateSet &states, bool ungrab);
    void resumeSucceeded();

    bool timeoutReached() const;
    void resetTimeout();

    void sendKillToAllInstances(bool keepOneSuccessful, unsigned procId);
    bool killOnTimeOut();
    bool resumeSucceededState(S2EExecutionState *s);

    bool killAllButOneSuccessful();
    void killAllButOneSuccessfulLocal(bool ungrabLock);


    void onCustomInstruction(S2EExecutionState* state,
        uint64_t opcode);

    void checkInvariants(bool grabLock=false);

    void suspendCurrentProcess();
    void resumeAllProcesses();
    unsigned getSuspendedProcessCount();

public:
    bool succeedState(S2EExecutionState *s);

    //Checks whether the search has no more states
    bool empty();

    friend void sm_callback(S2EExecutionState *s, bool killingState);
};


}
}

#endif
