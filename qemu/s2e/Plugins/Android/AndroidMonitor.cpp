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
 *    Andreas Kirchner <akalypse@gmail.com>
 *
 * All contributors are listed in S2E-AUTHORS file.
 *
 */

/**
 *  This plugin allows to trace various Android events emitted over custom instructions
 *
 *  RESERVES THE CUSTOM OPCODE 0xBB
 */


extern "C" {
#include "config.h"
#include "qemu-common.h"
}


#include <s2e/S2E.h>
#include <s2e/Utils.h>
#include <s2e/ConfigFile.h>
#include <s2e/Plugins/Opcodes.h>
#include <s2e/Plugins/Android/AndroidMonitor.h>

#include <sstream>

using namespace std;
using namespace s2e;
using namespace s2e::android;
using namespace s2e::linuxos;
using namespace s2e::plugins;

namespace s2e {
namespace plugins {

S2E_DEFINE_PLUGIN(AndroidMonitor, "Plugin for tracing Android events", "AndroidMonitor", "LinuxMonitor", "ModuleExecutionDetector", "FunctionMonitor");

AndroidMonitor::~AndroidMonitor()
{

}

void AndroidMonitor::initialize()
{
    std::vector<std::string> Sections;
    Sections = s2e()->getConfig()->getListKeys(getConfigKey());
    bool ok = false;

    aut_process_name = s2e()->getConfig()->getString(getConfigKey() + ".app_process_name", "", &ok).c_str();

    if (!ok) {
        s2e()->getWarningsStream() << "AndroidMonitor: No process name for application under test provided."
            <<std::endl;
    }

    m_linuxMonitor = static_cast<LinuxMonitor*>(s2e()->getPlugin("LinuxMonitor"));
    assert(m_linuxMonitor);

    m_modulePlugin = static_cast<ModuleExecutionDetector*>(s2e()->getPlugin("ModuleExecutionDetector"));
    assert(m_modulePlugin);

    s2e()->getCorePlugin()->onCustomInstruction.connect(
            sigc::mem_fun(*this, &AndroidMonitor::onCustomInstruction));
    m_linuxMonitor->onProcessFork.connect(
            sigc::mem_fun(*this, &AndroidMonitor::onProcessFork));
    m_linuxMonitor->onSyscall.connect(
    		sigc::mem_fun(*this, &AndroidMonitor::onSyscall));
    m_linuxMonitor->onNewProcess.connect(
    		sigc::mem_fun(*this, &AndroidMonitor::onNewProcess));
    m_linuxMonitor->onNewThread.connect(
    		sigc::mem_fun(*this, &AndroidMonitor::onNewThread));
    m_modulePlugin->onModuleTransition.connect(
    		sigc::mem_fun(*this, &AndroidMonitor::onModuleTransition));

    m_funcMonitor = static_cast<FunctionMonitor*>(s2e()->getPlugin("FunctionMonitor"));
    assert(m_funcMonitor);

    swivec_connected = false;

    s2e()->getCorePlugin()->onTranslateBlockStart.connect(
               sigc::mem_fun(*this, &AndroidMonitor::slotTranslateBlockStart));
}

void AndroidMonitor::onCustomInstruction(S2EExecutionState* state, uint64_t opcode)
{
    if (!OPCODE_CHECK(opcode, ANDROID_MONITOR_OPCODE)) {
        return;
    }

//    uint8_t opc = (opcode>>16) & 0xFF;
//    if (opc != 0xBB) {
//        return;
//    }

    uint8_t op = (opcode>>8) & 0xFF;

    switch(op) {
    case 0: { /* trace location */
    	std::string message;
    	uint32_t messagePtr;
    	bool ok = true;
    	ok &= state->readCpuRegisterConcrete(CPU_OFFSET(regs[0]), &messagePtr, 4);
    	if (!ok) {
    	       s2e()->getWarningsStream(state)
    	       << "ERROR: symbolic argument was passed to s2e_op android monitor"
    	       << std::endl;
    	} else {
    		message="<NO MESSAGE>";
    	       if(messagePtr && !state->readString(messagePtr, message)) {
    	            s2e()->getWarningsStream(state)
    	             << "Error reading detail informations about location from the guest" << std::endl;
    	       }
    	}
    	s2e()->getMessagesStream(state) << "Location traced with info: " << message << std::endl;
    	break;
    }
    case 1: { /* trace uid */
    	std::string message;
    	uint32_t messagePtr;
    	bool ok = true;
    	ok &= state->readCpuRegisterConcrete(CPU_OFFSET(regs[0]), &messagePtr, 4);
    	if (!ok) {
    	       s2e()->getWarningsStream(state)
    	       << "ERROR: symbolic argument was passed to s2e_op android monitor"
    	       << std::endl;
    	} else {
        	   message="<NO MESSAGE>";
    	       if(messagePtr && !state->readString(messagePtr, message)) {
    	            s2e()->getWarningsStream(state)
    	             << "Error reading detail informations about location from the guest" << std::endl;
    	       }
    	}
    	s2e()->getMessagesStream(state) << "UID traced with info: " << message << std::endl;
    	break;
    }
    default:
    	break;
    }
}

void AndroidMonitor::onProcessFork(S2EExecutionState *state, uint32_t clone_flags, linux_task* task) {

	s2e()->getDebugStream() << "AndroidMonitor: Fork details: " << "Clone Flags: " << hex << clone_flags << endl << LinuxMonitor::dumpTask(task) << endl;
	if(!task->comm.compare("zygote")) {
		linux_task appthread = *task;
		if (clone_flags == 0x11) {
			s2e()->getDebugStream() << "Main process of Android application started. Waiting for information over custom instruction..." << endl;
			assert(appthread.pid == appthread.tgid);
			aut.process = appthread;

		} else if (clone_flags == 0x450F00) {
			s2e()->getDebugStream() << "Further thread of new Android application started.  Waiting for information over custom instruction..." << endl;
			linux_task appthread = *task;
			assert(appthread.pid != appthread.tgid);
			aut.threads.insert(appthread);
		} else  {
			s2e()->getDebugStream() << "Zygote fork with unrecognized  clone flags:" << hex << clone_flags << endl;
		}
	}

	if (task->tgid == aut.process.tgid && !task->comm.compare("Binder Thread #")) {
		s2e()->getDebugStream() << "Binder Thread of Android application started." << endl;
		linux_task binderthread = *task;
		aut.threads.insert(binderthread);
	}

	/*
	 * find all defs in /kernel-common/include/linux/sched.h
	 * a thread
	 *     	 0x4000 CLONE_VFORK
	 *	 	 0x0400 CLONE_FILES
	 * 		 0x0100 CLONE_VM
	 *	 	 0x00FF CSIGNAL
	 *	 	 0x0011 SIGCHLD
	 *
	 *
	 *	1. Zygote forks the process (clone_flags = SIGCHLD).
	 *	2. Additional 4 threads (clone_flags = CLONE_VFORK & CLONE_FILES & CLONE_VM & CSIGNAL) are created. c.f., dalvik/vm/Init.c:dvmInitAfterZygote(..)
	 *	   - Signal Catcher Thread that dumps stack after SIGQUIT dvmSignalCatcherStartup()
	 *	   - Stdout/StdErr copier Thread: dvmStdioConverterStartup()
	 *	   - JDWP thread for debugger: dvmInitJDWP()
	 *	   - if JIT enabled: start a thread for the JIT compiler dvmCompilerStartup()
	 *	   --> every thread uses: dvmCreateInternalThread(..)
	 *	3. A binder thread is created
	 *  4. Syscall 5 (open)
	 */
}

void AndroidMonitor::onNewThread(S2EExecutionState *state, linux_task* thread, linux_task* process) {
	if (!process->comm.compare("system_server")) {
		s2e()->getDebugStream() << "AndroidMonitor:: system_server thread detected with pid: " << dec << thread->pid << endl;
		linux_task newThread = *process;
		systemServer.threads.insert(newThread);
		return;
	}
	if (!process->comm.compare("zygote")) {
		s2e()->getDebugStream() << "AndroidMonitor:: application thread detected with pid: " << dec << thread->pid << endl;
//		linux_task newThread = *process;
//		zygote.threads.insert(newThread);
		return;
	}

}
void AndroidMonitor::onNewProcess(S2EExecutionState *state, linux_task* process) {

	if (!process->comm.compare("system_server")) {
		s2e()->getDebugStream() << "AndroidMonitor:: system_server process detected with pid: " << dec << process->pid << endl;
		linux_task newProcess = *process;
		systemServer.process = newProcess;
		return;
	}

	if (!process->comm.compare("zygote")) {
		s2e()->getDebugStream() << "AndroidMonitor:: zygote detected with pid: " << dec << process->pid << endl;
		linux_task newProcess = *process;
		zygote.process = newProcess;
		return;
	}

}

void AndroidMonitor::onSyscall(S2EExecutionState *state, uint32_t syscall_nr) {
	s2e()->getDebugStream() << "AndroidMonitor:: syscall observed: " << hex << syscall_nr << endl;
}

void AndroidMonitor::onModuleTransition(S2EExecutionState* state, const ModuleDescriptor *previousModule, const ModuleDescriptor *nextModule) {
	if(nextModule == NULL) {
		return;
	}

	if(nextModule->Name.find(aut_process_name) != string::npos) {
		s2e()->getMessagesStream() << "AndroidMonitor:: enter " << nextModule->Name << endl;
	}
	if(nextModule->Name.find("libs2ewrapper.so") != string::npos) {
		s2e()->getMessagesStream() << "AndroidMonitor:: enter " << nextModule->Name << endl;
	}

}


void AndroidMonitor::slotTranslateBlockStart(ExecutionSignal *signal, S2EExecutionState *state, TranslationBlock *tb, uint64_t pc) {

    if (swivec_connected) {
        return;
    }

    FunctionMonitor::CallSignal *callSignal;

    //SWI exception vector
    //XXX: put it config file?
	s2e::linuxos::symbol_struct irq_usr;
	bool ok = m_linuxMonitor->searchSymbol("__irq_usr",irq_usr);
	if(!ok) {
		s2e()->getDebugStream() << "AndroidMonitor:: Could not find required symbol '__irq_usr'" << endl;
		exit(-1);
	}
	assert(irq_usr.adr);

	// Register a call signal
	callSignal = m_funcMonitor->getCallSignal(state, irq_usr.adr, -1);

	// Register signal handler
	callSignal->connect(sigc::mem_fun(*this, &AndroidMonitor::swiHook));
	swivec_connected = true;

}

void AndroidMonitor::swiHook(S2EExecutionState* state, FunctionMonitorState *fns) {

	//Test: We skip SWI's when something is symbolic
	uint64_t smask = state->getSymbolicRegistersMask();
	if (smask) {
		uint32_t lr;
		state->readCpuRegisterConcrete(offsetof(CPUState, regs[14]), &lr, 4);
		s2e()->getMessagesStream() << "AndroidMonitor:: SWI from " << hex << lr << dec << "." << endl;
//		s2e::linuxos::symbol_struct ret_to_user;
//		bool ok = m_linuxMonitor->searchSymbol("ret_to_user",ret_to_user);
//		if(!ok) {
//			s2e()->getDebugStream() << "AndroidMonitor:: Could not find required symbol 'ret_to_user'" << endl;
//			exit(-1);
//		}
//		//skip the interrupt_handling and jump directly to macro ret_to_user
//		state->writeCpuState(offsetof(CPUState, regs[15]), ret_to_user.adr, 8*sizeof(uint32_t));
	}
}


} // namespace plugins
} // namespace s2e
