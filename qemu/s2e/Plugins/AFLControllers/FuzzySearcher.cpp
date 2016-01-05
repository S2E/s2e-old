/*
 * S2E Selective Symbolic Execution Framework
 *
 * Copyright (c) 2015, Information Security Laboratory, NUDT
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
 * All contributors are listed in S2E-AUTHORS file.
 *
 */

#include "FuzzySearcher.h"
extern "C" {
#include <qemu-common.h>
#include <cpu-all.h>
#include <exec-all.h>
#include <sysemu.h>
#include <sys/shm.h>
}
#include <s2e/S2E.h>
#include <s2e/S2EExecutor.h>
#include <s2e/ConfigFile.h>
#include <s2e/Utils.h>

#include <iomanip>
#include <cctype>

#include <algorithm>
#include <fstream>
#include <vector>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>    /**/
#include <errno.h>     /*errno*/
#include <unistd.h>    /*ssize_t*/
#include <sys/types.h>
#include <sys/stat.h>  /*mode_t*/
#include <stdlib.h>

using namespace llvm::sys;

//#define DEBUG

extern "C" void kbd_put_keycode(int keycode);

std::string g_inicasepool = "/tmp/inipool/";
std::string g_curcasepool = "/tmp/curpool/";

namespace s2e {
namespace plugins {

S2E_DEFINE_PLUGIN(FuzzySearcher, "FuzzySearcher plugin", "FuzzySearcher",
        "ModuleExecutionDetector", "HostFiles");

FuzzySearcher::~FuzzySearcher()
{
}
void FuzzySearcher::initialize()
{
    bool ok = false;
    std::string cfgkey = getConfigKey();
    //afl related
    m_aflOutputpool = s2e()->getConfig()->getString(cfgkey + ".aflOutput", "",
            &ok);
    if (!ok) {
        s2e()->getDebugStream()
                << "FuzzySearcher: You should specify AFL's output directory as aflOutputpool\n";
        exit(0);
    }
    m_aflRoot = s2e()->getConfig()->getString(cfgkey + ".aflRoot", "", &ok);
    if (!ok) {
        s2e()->getDebugStream()
                << "FuzzySearcher: You should specify AFL's root directory as aflRoot\n";
        exit(0);
    }
    m_aflAppArgs = s2e()->getConfig()->getString(cfgkey + ".aflAppArgs", "",
            &ok);
    if (!ok) {
        s2e()->getDebugStream()
                << "FuzzySearcher: You should specify target binary's full arguments as aflAppArgs\n";
        exit(0);
    }
    m_aflBinaryMode = s2e()->getConfig()->getBool(
            getConfigKey() + ".aflBinaryMode", false, &ok);
    m_MAXLOOPs = s2e()->getConfig()->getInt(getConfigKey() + ".MaxLoops", 10,
            &ok);
    m_verbose = s2e()->getConfig()->getBool(getConfigKey() + ".debugVerbose",
            false, &ok);
    m_symbolicfilename = s2e()->getConfig()->getString(
            cfgkey + ".symbolicfilename", "testcase", &ok);
    m_inicasepool = s2e()->getConfig()->getString(cfgkey + ".inicasepool",
            g_inicasepool, &ok);
    m_curcasepool = s2e()->getConfig()->getString(cfgkey + ".curcasepool",
            g_curcasepool, &ok);
    //find the path
    if (access(m_inicasepool.c_str(), F_OK)) {
        std::cerr << "Could not find directory " << m_inicasepool << '\n';
        exit(0);
    }
    if (access(m_curcasepool.c_str(), F_OK)) {
        std::cerr << "Could not find directory " << m_curcasepool << '\n';
        exit(0);
    }

    std::string mkdirError;
    std::string generated_dir = s2e()->getOutputDirectory() + "/testcases";
    Path generateDir(generated_dir);
#ifdef _WIN32
    if (generateDir.createDirectoryOnDisk(false, &mkdirError)) {
#else
    if (generateDir.createDirectoryOnDisk(true, &mkdirError)) {
#endif
        std::cerr << "Could not create directory " << generateDir.str()
                << " error: " << mkdirError << '\n';
    }

    m_mainModule = s2e()->getConfig()->getString(cfgkey + ".mainModule",
            "MainModule", &ok);

    m_detector = static_cast<ModuleExecutionDetector*>(s2e()->getPlugin(
            "ModuleExecutionDetector"));
    if (!m_detector) {
        std::cerr << "Could not find ModuleExecutionDetector plug-in. " << '\n';
        exit(0);
    }
    m_hostFiles = static_cast<HostFiles*>(s2e()->getPlugin("HostFiles"));
    m_AutoShFileGenerator = static_cast<AutoShFileGenerator*>(s2e()->getPlugin(
            "AutoShFileGenerator"));
    //
    m_autosendkey_enter = s2e()->getConfig()->getBool(
            getConfigKey() + ".autosendkey_enter", false, &ok);
    m_autosendkey_interval = s2e()->getConfig()->getInt(
            getConfigKey() + ".autosendkey_interval", 10, &ok);
    m_firstInstructionTranslateStart =
            s2e()->getCorePlugin()->onTranslateInstructionStart.connect(
                    sigc::mem_fun(*this,
                            &FuzzySearcher::slotFirstInstructionTranslateStart));
    s2e()->getCorePlugin()->onStateSwitchEnd.connect(
    sigc::mem_fun(*this, &FuzzySearcher::onStateSwitchEnd));
    s2e()->getCorePlugin()->onStateKill.connect(
    sigc::mem_fun(*this, &FuzzySearcher::onStateKill));
    m_detector->onModuleLoad.connect(
    sigc::mem_fun(*this, &FuzzySearcher::onModuleLoad));
    m_detector->onModuleTranslateBlockStart.connect(
    sigc::mem_fun(*this, &FuzzySearcher::onModuleTranslateBlockStart));
    m_detector->onModuleTranslateBlockEnd.connect(
    sigc::mem_fun(*this, &FuzzySearcher::onModuleTranslateBlockEnd));
    s2e()->getExecutor()->setSearcher(this);
}

//return *states[theRNG.getInt32()%states.size()];

klee::ExecutionState& FuzzySearcher::selectState()
{
    klee::ExecutionState *state;
    if (!m_speculativeStates.empty()) { //to maximum random, priority to speculative state
        States::iterator it = m_speculativeStates.begin();
        int random_index = rand() % m_speculativeStates.size(); //random select a testcase
        while (random_index) {
            it++;
            random_index--;
        }
        state = *it;
        if (state->m_is_carry_on_state) { // we cannot execute carry on state, because it is the seed
            if (m_speculativeStates.size() > 1) {
                ++it;
                if (it == m_speculativeStates.end()) {
                    it--;
                    it--;
                }
                state = *it;
            } else if (!m_normalStates.empty()) {
                state = *m_normalStates.begin();
            }
        }
    } else {
        assert(!m_normalStates.empty());
        States::iterator it = m_normalStates.begin();
        int random_index = rand() % m_normalStates.size();
        while (random_index) {
            it++;
            random_index--;
        }
        state = *it;
        if (state->m_is_carry_on_state) { //we cannot execute carry on state, because it is the seed
            if (m_normalStates.size() > 1) {
                ++it;
                if (it == m_normalStates.end()) {
                    it--;
                    it--;
                }
                state = *it;
            }
        }
    }

    if (state->m_silently_concretized) { // we don't want the silently concretized state
        s2e()->getDebugStream()
                << "FuzzySearcher: we are in a silently concretized state\n";
        if (m_normalStates.find(state) != m_normalStates.end()) {
            m_normalStates.erase(state);
            if (m_normalStates.empty())
                s2e()->getDebugStream()
                        << "FuzzySearcher: m_normalStates's size is " << "0"
                        << "\n";
            else
                s2e()->getDebugStream()
                        << "FuzzySearcher: m_normalStates's size is"
                        << m_normalStates.size() << "\n";
        } else {
            m_speculativeStates.erase(state);
            if (m_speculativeStates.empty())
                s2e()->getDebugStream()
                        << "FuzzySearcher: m_speculativeStates's size is "
                        << "0" << "\n";
            else
                s2e()->getDebugStream()
                        << "FuzzySearcher: m_speculativeStates's size is"
                        << m_speculativeStates.size() << "\n";
        }
        return selectState();
    }
    return *state;
}

void FuzzySearcher::update(klee::ExecutionState *current,
        const std::set<klee::ExecutionState*> &addedStates,
        const std::set<klee::ExecutionState*> &removedStates)
{
    if (current && addedStates.empty() && removedStates.empty()) {
        S2EExecutionState *s2estate = dynamic_cast<S2EExecutionState*>(current);
        if (!s2estate->isZombie()) {
            if (current->isSpeculative()) {
                m_normalStates.erase(current);
                m_speculativeStates.insert(current);
            } else {
                m_speculativeStates.erase(current);
                m_normalStates.insert(current);
            }
        }
    }

    foreach2(it, removedStates.begin(), removedStates.end())
    {
        if (*it == NULL)
            continue;
        S2EExecutionState *es = dynamic_cast<S2EExecutionState*>(*it);
        if (es->isSpeculative()) {
            m_speculativeStates.erase(es);
            s2e()->getDebugStream() << "m_speculativeStates.erase --- 2\n";
        } else {
            m_normalStates.erase(es);
            s2e()->getDebugStream() << "m_normalStates.erase --- 2\n";
        }
    }

    foreach2(it, addedStates.begin(), addedStates.end())
    {
        if (*it == NULL)
            continue;
        S2EExecutionState *es = dynamic_cast<S2EExecutionState*>(*it);
        if (es->isSpeculative()) {
            m_speculativeStates.insert(es);
            s2e()->getDebugStream()
                    << "FuzzySearcher: Insert speculative State, m_speculativeStates' size is "
                    << m_speculativeStates.size() << ", state is"
                    << (es->m_is_carry_on_state ? "" : " not")
                    << " carry on state\n";
        } else {
            m_normalStates.insert(es);
            s2e()->getDebugStream()
                    << "FuzzySearcher: Insert normal State, m_normalStates' size is "
                    << m_normalStates.size() << ", state is"
                    << (es->m_is_carry_on_state ? "" : " not")
                    << " carry on state\n";
        }
    }

}
bool FuzzySearcher::empty()
{
    return m_normalStates.empty() && m_speculativeStates.empty();
}

void FuzzySearcher::slotFirstInstructionTranslateStart(ExecutionSignal *signal,
        S2EExecutionState *state, TranslationBlock *tb, uint64_t pc)
{
    if (!m_isfirstInstructionProcessed) {
        m_firstInstructionProcess = signal->connect(
        sigc::mem_fun(*this, &FuzzySearcher::ProcessFirstInstruction)); // we start from the very beginning of this state
    }
}
void FuzzySearcher::ProcessFirstInstruction(S2EExecutionState* state,
        uint64_t pc)
{
    if (!m_isfirstInstructionProcessed) {
        prepareNextState(state, true);
        m_isfirstInstructionProcessed = true;
        m_firstInstructionProcess.disconnect(); // we only do this once time
        m_firstInstructionTranslateStart.disconnect(); // we only do this once time
    }
}
void FuzzySearcher::onStateSwitchEnd(S2EExecutionState *currentState,
        S2EExecutionState *nextState)
{
    s2e()->getDebugStream()
            << "FuzzySearcher: Capture state switch end event, give a chance to handle it.\n";
    // terminate the last state and generate testcase
    if (currentState && !(currentState->m_is_carry_on_state)) {
        s2e()->getExecutor()->terminateStateEarly(*currentState,
                "FuzzySearcher: terminate this for fuzzing");
    }
    if (currentState && (currentState->m_silently_concretized)) {
        //s2e()->getExecutor()->terminateStateEarly(*currentState, "FuzzySearcher: terminate silently concretized state");
    }
    // if S2E only has the carry on state, then carry on to next iteration
    if (nextState && nextState->m_is_carry_on_state) {
        if (m_verbose) {
            s2e()->getDebugStream()
                    << "FuzzySearcher: We only have the seed state, now fetching new testcase.\n";
        }
        /*
         * AFLROOT=/home/epeius/work/afl-1.96b
         * $AFLROOT/afl-fuzz -m 4096M -t 50000 -i /home/epeius/work/DSlab.EPFL/FinalTest/evincetest/seeds/ -o /home/epeius/work/DSlab.EPFL/FinalTest/evincetest/res/ -Q /usr/bin/evince @@
         */
        if (!m_AFLStarted) { //if AFL not started, then starts it
            std::stringstream aflCmdline;
            std::string generated_dir = s2e()->getOutputDirectory()
                    + "/testcases";
            aflCmdline << m_aflRoot << "afl-fuzz -m 4096M -t 5000 -i "
                    << generated_dir << " -o " << m_aflOutputpool
                    << (m_aflBinaryMode ? " -Q " : "") << m_aflAppArgs << " &";
            if (m_verbose) {
                s2e()->getDebugStream()
                        << "FuzzySearcher: AFL command line is: "
                        << aflCmdline.str() << "\n";
            }
            system(aflCmdline.str().c_str()); //we don't want to suspend it, so we add "&":
            m_AFLStarted = true;
            sleep(10); //let us wait for new testcases
        }

        assert(m_AFLStarted && "AFL has not started, why?");
        {
            std::stringstream ssAflQueue;
            ssAflQueue << m_aflOutputpool << "queue/";
            Path inicasepool_llvm(m_inicasepool);
            Path aflqueuecasepool_llvm(ssAflQueue.str());
            ClearbeforeCopyto(inicasepool_llvm, aflqueuecasepool_llvm);
        }
        prepareNextState(nextState);
    }
}

void FuzzySearcher::onStateKill(S2EExecutionState *currentState)
{
    //generate the testcase for AFL
    //step1. construct name and generate the testcase
    std::stringstream ssgeneratedCase;
    std::string destfilename;
    if (1) {
        ssgeneratedCase << "testcases/" << getCurrentLoop() << "-"
                << currentState->getID() << "-" << m_symbolicfilename; // Lopp-StateID-m_symbolicfilename
        destfilename = s2e()->getOutputFilename(ssgeneratedCase.str().c_str());
    } else {
        ssgeneratedCase << m_aflOutputpool << "queue/" << getCurrentLoop()
                << "-" << currentState->getID() << "-" << m_symbolicfilename; // Lopp-StateID-m_symbolicfilename
        destfilename = ssgeneratedCase.str();
    }
    if (m_verbose) {
        s2e()->getDebugStream() << "FuzzySearcher: Generating testcase: "
                << destfilename << ".\n";
    }
    generateCaseFile(currentState, destfilename);
}

klee::Executor::StatePair FuzzySearcher::prepareNextState(
        S2EExecutionState *state, bool isinitial)
{

    klee::Executor::StatePair sp;
    state->jumpToSymbolicCpp();
    bool oldForkStatus = state->isForkingEnabled();
    state->enableForking();
    if (isinitial) {
        m_dummy_symb = state->createSymbolicValue("dummy_symb_var",
                klee::Expr::Int32); // we need to create an initial state which can be used to continue execution
        m_current_conditon = 0;
    }
    //	printf("FuzzySearcher: prepareNextState\n");
    state->m_preparingstate = true;
    std::vector<klee::Expr> conditions;
    klee::ref<klee::Expr> cond = klee::NeExpr::create(m_dummy_symb,
            klee::ConstantExpr::create(m_current_conditon, klee::Expr::Int32));
    sp = s2e()->getExecutor()->fork(*state, cond, false);

    S2EExecutionState *ts = static_cast<S2EExecutionState *>(sp.first);
    S2EExecutionState *fs = static_cast<S2EExecutionState *>(sp.second);

    klee::ref<klee::Expr> condnot = klee::EqExpr::create(m_dummy_symb,
            klee::ConstantExpr::create(m_current_conditon, klee::Expr::Int32));
    fs->addConstraint(condnot);

    ts->setForking(oldForkStatus);
    fs->setForking(oldForkStatus);

    ts->m_is_carry_on_state = true;
    fs->m_is_carry_on_state = false;

    ts->m_preparingstate = false;
    fs->m_preparingstate = false;

    m_current_conditon++;

    if (m_loops >= m_MAXLOOPs) { //reach the maximum
        if (m_verbose) {
            s2e()->getDebugStream() << "FuzzySearcher: Ready to exit\n";
        }
        CleanAndQuit();
    }
    //fetch a new testcase from pool
    getNewCaseFromPool(fs);
    m_loops += 1;
    if (m_verbose) {
        s2e()->getDebugStream() << "FuzzySearcher: Ready to start " << m_loops
                << " iteration(s).\n";
    }

    m_key_enter_sent = false;
    CorePlugin *plg = s2e()->getCorePlugin();
    m_timerconn = plg->onTimer.connect(
    sigc::mem_fun(*this, &FuzzySearcher::onTimer));
    TimeValue curTime = TimeValue::now();
    m_currentTime = curTime.seconds();
    S2EExecutionState::resetLastSymbolicId();
    s2e()->getExecutor()->updateStates(state);
    return sp;
}

void FuzzySearcher::onTimer()
{
    TimeValue curTime = TimeValue::now();
    if (m_currentTime < (curTime.seconds() - m_autosendkey_interval)) { // wait a while so that S2E can capture this key
        m_currentTime = curTime.seconds();
        //auto send kp-enter to start iteration so that we do not need to do it manually.
        if (m_autosendkey_enter && !m_key_enter_sent) {
            int keycode = 0x9c; //kp_enter

            if (keycode & 0x80) {
                kbd_put_keycode(0xe0);
            }
            kbd_put_keycode(keycode & 0x7f);

            if (m_verbose) {
                s2e()->getDebugStream()
                        << "FuzzySearcher: Automatically sent kp_enter to QEMU.\n";
            }
            //kbd_put_keycode(keycode);
            m_key_enter_sent = true;
            m_timerconn.disconnect();
        }
    }

}

void FuzzySearcher::CleanAndQuit()
{
    try {
        char cmd[] = "pgrep -l afl-fuzz";
        FILE *pp = popen(cmd, "r");
        if (!pp) {
            s2e()->getDebugStream()
                    << "FuzzySearcher: Cannot open the pipe to read\n";
            exit(0);
        }
        char tmp[128]; //
        while (fgets(tmp, sizeof(tmp), pp) != NULL) {
            if (tmp[strlen(tmp) - 1] == '\n') {
                tmp[strlen(tmp) - 1] = '\0';
            }
            std::string str_tmp = tmp;
            str_tmp = str_tmp.substr(0, str_tmp.find_first_of(' '));
            int tmpPid = atoi(str_tmp.c_str());
            if (m_verbose) {
                s2e()->getDebugStream() << "FuzzySearcher: try to kill pid: "
                        << tmpPid << "\n";
            }
            kill(tmpPid, SIGKILL);
        }
        pclose(pp); //close pipe
    } catch (...) {
        s2e()->getDebugStream() << "FuzzySearcher: Cannot kill AFL, why?\n";
    }
    if (m_verbose) {
        s2e()->getDebugStream()
                << "FuzzySearcher: Reach the maxmium iteration, quitting...\n";
    }
    if (m_shmID) {
        // remove share memory
        std::stringstream IPCRMCmdline;
        IPCRMCmdline << "ipcrm -m " << m_shmID << " &";
        system(IPCRMCmdline.str().c_str());
    }
    qemu_system_shutdown_request(); //XXX:This invocation will cause illegal instruction (ud2) if there has no return for current function
    exit(0);
}

void FuzzySearcher::ClearbeforeCopyto(Path &dstDir, Path &srcDir)
{
    std::set<Path> taskfiles;
    std::set<Path> oldfiles;
    std::string ErrorMsg;
    // clear the destination folder
    if (!dstDir.getDirectoryContents(oldfiles, &ErrorMsg)) {
        typeof(oldfiles.begin()) it = oldfiles.begin();
        while (it != oldfiles.end()) {
            (*it).eraseFromDisk();
            it++;
        }
    }

    if (!srcDir.getDirectoryContents(taskfiles, &ErrorMsg)) {
        int taskcount = taskfiles.size();
        if (m_verbose) {
            s2e()->getDebugStream() << "FuzzySearcher: we found " << taskcount
                    << " testcase(s) in " << srcDir.c_str() << "\n";
        }
        typeof(taskfiles.begin()) it = taskfiles.begin();
        while (it != taskfiles.end()) {
            std::string fullfilename = (*it).str();
            const char *filename = basename(fullfilename.c_str());
            if (!strcmp(filename, ".")) {
                it++;
                continue;
            }
            if (!strcmp(filename, "..")) {
                it++;
                continue;
            }
            //std::string filename = (*it).filename();
            std::stringstream bckfile;
            bckfile << m_inicasepool.c_str() << filename;
            if (m_verbose) {
                s2e()->getDebugStream() << "FuzzySearcher: Copying filename: "
                        << (*it).str() << " from the queue to"
                                " filename: " << bckfile.str() << "\n";
            }
            copyfile(fullfilename.c_str(), bckfile.str().c_str());
            it++;
        }

    }
}

/**
 * Fetch a new testcase
 */
S2EExecutionState* FuzzySearcher::getNewCaseFromPool(S2EExecutionState* instate)
{
    bool done = false;
    int idlecounter = 0;
    Path inicasepool_llvm(m_inicasepool);
    Path curcasepool_llvm(m_curcasepool);
    std::set<Path> oldfiles;
    std::string ErrorMsg;
    // clear the current testcase pool
    if (!curcasepool_llvm.getDirectoryContents(oldfiles, &ErrorMsg)) {
        typeof(oldfiles.begin()) it = oldfiles.begin();
        while (it != oldfiles.end()) {
            (*it).eraseFromDisk();
            it++;
        }
    }
    while (!done) {
        //
        idlecounter++;
        std::set<Path> taskfiles;
        std::string ErrorMsg;
        if (!inicasepool_llvm.getDirectoryContents(taskfiles, &ErrorMsg)) {
            int taskcount = taskfiles.size();
            if (m_verbose) {
                s2e()->getDebugStream() << "FuzzySearcher: Find " << taskcount
                        << " testcases in the queue.\n";
            }
            if (taskcount <= 0) {
                //pass
            } else {
                int selectindex = rand() % taskcount;
                typeof(taskfiles.begin()) it = taskfiles.begin();
                while (it != taskfiles.end()) {
                    std::string fullfilename = (*it).str();
                    const char *filename = basename(fullfilename.c_str());
                    if (!filename) {
                        s2e()->getDebugStream()
                                << "FuzzySearcher: Could not allocate memory for file basename ---2.\n";
                        exit(0); // must be interrupted
                    }
                    if (!strcmp(filename, ".") || !strcmp(filename, "..")) {
                        it++;
                        continue;
                    } else {
                        if (!selectindex) {
                            try { // try to fetch this testcase and rename it so that the s2e guest can get it
                                std::stringstream bckfile;
                                bckfile << m_curcasepool.c_str()
                                        << m_symbolicfilename;
                                if (m_verbose) {
                                    s2e()->getDebugStream()
                                            << "FuzzySearcher: Copying filename: "
                                            << fullfilename
                                            << " from the queue to filename: "
                                            << bckfile.str() << "\n";
                                }
                                copyfile(fullfilename.c_str(),
                                        bckfile.str().c_str());
                                done = true;
                                break;
                            } catch (...) {
                                it++;
                            }
                        } else {
                            selectindex -= 1;
                            it++;
                        }
                    }
                }

            }
        }
        if (!done)
            sleep(1);
    }
    return instate;
}

bool FuzzySearcher::generateCaseFile(S2EExecutionState *state,
        std::string destfilename)
{
    //copy out template file to destination file
    std::stringstream template_file;
    template_file << m_curcasepool.c_str() << m_symbolicfilename;
    if (!copyfile(template_file.str().c_str(), destfilename.c_str()))
        return false;
    //try to solve the constraint and write the result to destination file
    int fd = open(destfilename.c_str(), O_RDWR);
    if (fd < 0) {
        s2e()->getDebugStream() << "could not open dest file: "
                << destfilename.c_str() << "\n";
        close(fd);
        return false;
    }
    /* Determine the size of the file */
    off_t size = lseek(fd, 0, SEEK_END);
    if (size < 0) {
        s2e()->getDebugStream() << "could not determine the size of :"
                << destfilename.c_str() << "\n";
        close(fd);
        return false;
    }

    off_t offset = 0;
    std::string delim_str = "_";
    const char *delim = delim_str.c_str();
    char *p;
    char maxvarname[1024] =
    { 0 };
    ConcreteInputs out;
    //HACK: we have to create a new temple state, otherwise getting solution in half of a state may drive to crash
    klee::ExecutionState* exploitState = new klee::ExecutionState(*state);
    bool success = s2e()->getExecutor()->getSymbolicSolution(*exploitState,
            out);

    if (!success) {
        s2e()->getWarningsStream() << "Could not get symbolic solutions"
                << '\n';
        return false;
    }
    ConcreteInputs::iterator it;
    for (it = out.begin(); it != out.end(); ++it) {
        const VarValuePair &vp = *it;
        std::string varname = vp.first;
        // "__symfile___%s___%d_%d_symfile__value_%02x",filename, offset,size,buffer[buffer_i]);
        //parse offset
        strcpy(maxvarname, varname.c_str());
        if ((strstr(maxvarname, "symfile__value"))) {
            strtok(maxvarname, delim);
            strtok(NULL, delim);
            strtok(NULL, delim);
            //strtok(NULL, delim);
            p = strtok(NULL, delim);
            offset = atol(p);
            if (lseek(fd, offset, SEEK_SET) < 0) {
                s2e()->getDebugStream() << "could not seek to position : "
                        << offset << "\n";
                close(fd);
                return false;
            }
        } else if ((strstr(maxvarname, "___symfile___"))) {
            //v1___symfile___E:\case\huplayerpoc.m3u___27_2_symfile___0: 1a 00, (string) ".."
            //__symfile___%s___%d_%d_symfile__
            strtok(maxvarname, delim);
            strtok(NULL, delim);
            strtok(NULL, delim);
            //strtok(NULL, delim);
            p = strtok(NULL, delim);
            offset = atol(p);
            if (lseek(fd, offset, SEEK_SET) < 0) {
                s2e()->getDebugStream() << "could not seek to position : "
                        << offset << "\n";
                close(fd);
                return false;
            }
        } else {
            continue;
        }
        unsigned wbuffer[1] =
        { 0 };
        for (unsigned i = 0; i < vp.second.size(); ++i) {
            wbuffer[0] = (unsigned) vp.second[i];
            ssize_t written_count = write(fd, wbuffer, 1);
            if (written_count < 0) {
                s2e()->getDebugStream() << " could not write to file : "
                        << destfilename.c_str() << "\n";
                close(fd);
                return false;
            }
        }
    }
    close(fd);
    return true;
}

#define BUFFER_SIZE 4
bool FuzzySearcher::copyfile(const char* fromfile, const char* tofile)
{
    int from_fd, to_fd;
    int bytes_read, bytes_write;
    char buffer[BUFFER_SIZE];
    char *ptr;

    if ((from_fd = open(fromfile, O_RDONLY)) == -1) /*open file readonly*/
    {
        fprintf(stderr, "Open %s Error:%s\n", fromfile, strerror(errno));
        return false;
    }
    if ((to_fd = open(tofile, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR)) == -1) {
        fprintf(stderr, "Open %s Error:%s\n", tofile, strerror(errno));
        return false;
    }
    while ((bytes_read = read(from_fd, buffer, BUFFER_SIZE))) {
        if ((bytes_read == -1) && (errno != EINTR))
            break;
        else if (bytes_read > 0) {
            ptr = buffer;
            while ((bytes_write = write(to_fd, ptr, bytes_read))) {
                if ((bytes_write == -1) && (errno != EINTR))
                    break;
                else if (bytes_write == bytes_read)
                    break;
                else if (bytes_write > 0) {
                    ptr += bytes_write;
                    bytes_read -= bytes_write;
                }
            }
            if (bytes_write == -1)
                break;
        }
    }
    close(from_fd);
    close(to_fd);
    return true;
}

void FuzzySearcher::onModuleLoad(S2EExecutionState* state,
        const ModuleDescriptor &md)
{

}

void FuzzySearcher::onModuleTranslateBlockStart(ExecutionSignal* es,
        S2EExecutionState* state, const ModuleDescriptor &mod,
        TranslationBlock* tb, uint64_t pc)
{
    if (!tb) {
        return;
    }
    if (m_mainModule == mod.Name) {
        es->connect(
        sigc::mem_fun(*this, &FuzzySearcher::slotExecuteBlockStart));
    }

}
void FuzzySearcher::onModuleTranslateBlockEnd(ExecutionSignal *signal,
        S2EExecutionState* state, const ModuleDescriptor &module,
        TranslationBlock *tb, uint64_t endPc, bool staticTarget,
        uint64_t targetPc)
{
    if (!tb) {
        return;
    }
    if (m_mainModule == module.Name) {
        signal->connect(
        sigc::mem_fun(*this, &FuzzySearcher::slotExecuteBlockEnd));
    }

}
/**
 */
void FuzzySearcher::slotExecuteBlockStart(S2EExecutionState *state, uint64_t pc)
{
    const ModuleDescriptor *curMd = m_detector->getCurrentDescriptor(state);
    if (!curMd) {
        return;
    }
    if (m_verbose) {
        s2e()->getDebugStream(state)
                << "FuzzySearcher: Find module when executing, we are in "
                << curMd->Name << ", current BB is: " << hexval(pc) << ".\n";
    }
    // do work here.
    if (!m_AFLStarted)			//let AFL create the bitmap
        return;
    if (!m_findBitMapSHM) {
        m_findBitMapSHM = getAFLBitmapSHM();
    } else {
        assert(m_aflBitmapSHM && "AFL's bitmap is NULL, why??");
        DECLARE_PLUGINSTATE(FuzzySearcherState, state);
        /*
         * cur_location = (block_address >> 4) ^ (block_address << 8);
         shared_mem[cur_location ^ prev_location]++;
         prev_location = cur_location >> 1;
         */
#ifdef DEBUG
        s2e()->getDebugStream(state)
        << "FuzzySearcher: Ready to update AFL bitmap.\n";
        bool success = plgState->updateAFLBitmapSHM(m_aflBitmapSHM, pc);
        if (success)
        s2e()->getDebugStream(state)
        << "FuzzySearcher: Successfully updated AFL bitmap.\n";
        else
        s2e()->getDebugStream(state)
        << "FuzzySearcher: Failed to update AFL bitmap.\n";
#else
        plgState->updateAFLBitmapSHM(m_aflBitmapSHM, pc);
#endif
    }
}

void FuzzySearcher::slotExecuteBlockEnd(S2EExecutionState *state, uint64_t pc)
{

}

bool FuzzySearcher::getAFLBitmapSHM()
{
    m_aflBitmapSHM = NULL;
    key_t shmkey;
    do {
        if ((shmkey = ftok("/tmp/aflbitmap", 1)) < 0) {
            s2e()->getDebugStream() << "FuzzySearcher: ftok() error: "
                    << strerror(errno) << "\n";
            return false;
        }
        int shm_id;
        try {
            if (!m_findBitMapSHM)
                sleep(5);			//wait afl for a while
            shm_id = shmget(shmkey, AFL_BITMAP_SIZE, IPC_CREAT | 0600);
            if (shm_id < 0) {
                s2e()->getDebugStream() << "FuzzySearcher: shmget() error: "
                        << strerror(errno) << "\n";
                return false;
            }
            void * afl_area_ptr = shmat(shm_id, NULL, 0);
            if (afl_area_ptr == (void*) -1) {
                s2e()->getDebugStream() << "FuzzySearcher: shmat() error: "
                        << strerror(errno) << "\n";
                exit(1);
            }
            m_aflBitmapSHM = (unsigned char*) afl_area_ptr;
            m_findBitMapSHM = true;
            m_shmID = shm_id;
            if (m_verbose) {
                s2e()->getDebugStream() << "FuzzySearcher: Share memory id is "
                        << shm_id << "\n";
            }
        } catch (...) {
            return false;
        }
    } while (0);
    return true;
}

bool FuzzySearcherState::updateAFLBitmapSHM(unsigned char* AflBitmap,
        uint64_t curBBpc)
{
    uint64_t cur_location = (curBBpc >> 4) ^ (curBBpc << 8);
    cur_location &= AFL_BITMAP_SIZE - 1;
    if (cur_location >= AFL_BITMAP_SIZE)
        return false;
    AflBitmap[cur_location ^ m_prev_loc]++;
    m_prev_loc = cur_location >> 1;
    return true;
}
FuzzySearcherState::FuzzySearcherState()
{
    m_plugin = NULL;
    m_state = NULL;
    m_prev_loc = 0;
}

FuzzySearcherState::FuzzySearcherState(S2EExecutionState *s, Plugin *p)
{
    m_plugin = static_cast<FuzzySearcher*>(p);
    m_state = s;
    m_prev_loc = 0;
}

FuzzySearcherState::~FuzzySearcherState()
{
}

PluginState *FuzzySearcherState::clone() const
{
    return new FuzzySearcherState(*this);
}

PluginState *FuzzySearcherState::factory(Plugin *p, S2EExecutionState *s)
{
    FuzzySearcherState *ret = new FuzzySearcherState(s, p);
    return ret;
}
}
} /* namespace s2e */
