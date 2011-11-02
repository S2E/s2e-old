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
#include <s2e/Plugins/Android/DalvikTools/OpCodeNames.h>
}


#include <s2e/S2E.h>
#include <s2e/Utils.h>
#include <s2e/ConfigFile.h>
#include <s2e/Plugins/Opcodes.h>
#include <s2e/Plugins/Android/AndroidMonitor.h>


#include <sstream>

//assumes bool ok;
#define GET_REGISTER_CONCRETE(_regno,_dest,_size) do {										\
		ok &= state->readCpuRegisterConcrete(CPU_OFFSET(regs[_regno]), &(_dest), _size);		\
		if (!ok) {																			\
		    s2e()->getWarningsStream(state)													\
		    << "ERROR: cannot read "<< #_dest << "from register "<< #_regno << "."			\
		    << std::endl;																	\
		    return;																			\
		}																					\
	}while(false)

#define GET_STRING_FROM_REGISTER(_regno,_dest,_default) do {								\
		uint32_t str_ptr;																	\
		GET_REGISTER_CONCRETE(_regno,str_ptr,4);											\
		_dest = _default;																	\
    	if(str_ptr && !state->readString(str_ptr, _dest)) {									\
    	    s2e()->getWarningsStream(state)													\
    	    << "Android Monitor:: Error reading string from guest ( "						\
			<< #_dest << " ). Use default: "<< _default << std::endl;						\
    	}																					\
    }while(false)

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
    aut_started = false;

    if (!ok) {
        s2e()->getWarningsStream() << "AndroidMonitor: No process name for application under test provided."
            <<std::endl;
    }

    m_linuxMonitor = static_cast<LinuxMonitor*>(s2e()->getPlugin("LinuxMonitor"));
    assert(m_linuxMonitor);

    m_modulePlugin = static_cast<ModuleExecutionDetector*>(s2e()->getPlugin("ModuleExecutionDetector"));
    assert(m_modulePlugin);

    m_tbtracer = NULL;

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

#if 0
    s2e()->getCorePlugin()->onException.connect(
    		sigc::mem_fun(*this, &AndroidMonitor::onException));
#endif

    m_funcMonitor = static_cast<FunctionMonitor*>(s2e()->getPlugin("FunctionMonitor"));
    assert(m_funcMonitor);

    swivec_connected = false;

    s2e()->getCorePlugin()->onTranslateBlockStart.connect(
               sigc::mem_fun(*this, &AndroidMonitor::slotTranslateBlockStart));
}


#if 0
#define EXCP_UDEF            1   /* undefined instruction */
#define EXCP_SWI             2   /* software interrupt */
#define EXCP_PREFETCH_ABORT  3
#define EXCP_DATA_ABORT      4
#define EXCP_IRQ             5
#define EXCP_FIQ             6
#define EXCP_BKPT            7
#define EXCP_EXCEPTION_EXIT  8   /* Return from v7M exception.  */
#define EXCP_KERNEL_TRAP     9   /* Jumped to kernel code page.  */
#define EXCP_STREX          10

void AndroidMonitor::onException(S2EExecutionState* state, unsigned type, uint64_t pc) {

}
#endif

void AndroidMonitor::onCustomInstruction(S2EExecutionState* state, uint64_t opcode)
{
    if (!OPCODE_CHECK(opcode, ANDROID_MONITOR_OPCODE)) {
        return;
    }

//    uint8_t opc = (opcode>>16) & 0xFF;
//    if (opc != 0xBB) {
//        return;
//    }

    uint8_t op = (opcode>>8) & 0xFF; // 0xFFBB0401 --> extract 04
    uint8_t op2 = (opcode) & 0xFF;   // 0xFFBB0401 --> extract 01

    switch(op) {
    case 0: { /* trace location */
    	std::string message;
    	bool ok = true;
    	GET_STRING_FROM_REGISTER(0,message,"<NO MESSAGE>");
    	s2e()->getMessagesStream(state) << "Location traced with info: " << message << std::endl;
    	break;
    }
    case 1: { /* trace uid */
    	std::string message;
    	bool ok = true;
    	GET_STRING_FROM_REGISTER(0,message,"<NO MESSAGE>");
    	s2e()->getMessagesStream(state) << "UID traced with info: " << message << std::endl;
    	break;
    }
    case 2: { /* app start */
    	std::string procName;
    	std::string compName;
    	uint32_t pid;
		int result = 0;
    	bool ok = true;
    	GET_STRING_FROM_REGISTER(0,procName,"<no procname>");
    	GET_REGISTER_CONCRETE(1,pid,4);
    	GET_STRING_FROM_REGISTER(2,compName,"<no compname>");

    	s2e()->getMessagesStream() << "AndroidMonitor:: new Android application started with "
    			<< "pid: " << pid << " process name: " << procName << " component name: " << compName << endl;

    	//do we want to analyze this application?
    	if(aut_process_name.compare(procName) == 0) {
    		//yes we do.
    		s2e::linuxos::linux_task myAppProcess;
    		m_linuxMonitor->searchForTask(state,pid,true,&myAppProcess);
    		myAppProcess.comm = procName;
    		aut.process = myAppProcess;
    		assert(myAppProcess.pid == pid);

    		aut_started = true;
    		result = 0xCAFE; //magic value understood by target-side of the plugin

        	s2e()->getMessagesStream() << "AndroidMonitor:: REGISTER application "
        			<< procName << " ( " << dec << pid << " )" << endl;

        	onAppStart.emit(state,&myAppProcess);
    	}

		state->writeCpuRegisterConcrete(CPU_OFFSET(regs[0]),&result,4);
    	break;
    }

    case 3: { /* on interpreted Instruction starts */

    	uint32_t tid;
    	uint32_t op;
    	uint32_t virtual_pc;
    	uint32_t virtual_fp;
    	uint32_t pc;

    	bool ok = true;

    	GET_REGISTER_CONCRETE(4,tid,4);
    	GET_REGISTER_CONCRETE(5,op,4);
    	GET_REGISTER_CONCRETE(6,virtual_pc,4);
    	GET_REGISTER_CONCRETE(7,virtual_fp,4);
    	GET_REGISTER_CONCRETE(14,pc,4);
//    	pc = (uint32_t) state->readCpuState(offsetof(CPUState, regs[15]), 8*sizeof(uint32_t));


    	OpCode opc = (OpCode)op;
    	const char * opc_name = "-n/a-";
    	if (opc <= OP_UNUSED_FF) {
    		 opc_name = getOpcodeName(opc);
    	}
    	s2e()->getDebugStream() << "AndroidMonitor(" << std::dec << tid << ") at pc "<< std::hex << pc << ": " << opc_name << " ( " << std::hex << op << " ) at vpc: " << virtual_pc << " vfp: " << virtual_fp << "." << std::endl;
    	break;
    }
    case 4: { /* on interpreted method call */
    	bool native = false;
    	if (op2 == 1) native = true;

    	uint32_t tid;
    	std::string callee; //method signature which is called
    	uint32_t virtual_pc;
    	uint32_t virtual_fp;

    	bool ok = true;

    	GET_REGISTER_CONCRETE(0,tid,4);
    	GET_STRING_FROM_REGISTER(1,callee,"<no method>");
    	GET_REGISTER_CONCRETE(2,virtual_pc,4);
    	GET_REGISTER_CONCRETE(3,virtual_fp,4);

    	s2e()->getDebugStream() << "AndroidMonitor(" << std::dec << tid << "): *** " << (native ? "NATIVE " : "") << " CALL " << "*** " << callee << " at vpc: " << std::hex << virtual_pc << " vfp: " << virtual_fp << "." << std::endl;
    	break;
    }

    case 5: { /* on interpreted method return */
    	uint32_t tid;
    	std::string fromMethod; //method which returns
    	std::string toMethod; //method which receives the return value and continues execution
    	uint32_t retvalAddr; //address to the address where the JValue retVal is stored.

    	bool ok = true;

    	GET_REGISTER_CONCRETE(0,tid,4);
    	GET_STRING_FROM_REGISTER(1,fromMethod,"<no fromMethod>");
    	GET_STRING_FROM_REGISTER(2,toMethod,"<no toMethod>");
    	GET_REGISTER_CONCRETE(3,retvalAddr,4);

    	s2e()->getDebugStream() << "AndroidMonitor(" << std::dec << tid << "): *** RETURN FROM *** " << fromMethod << ". Continue execution at " << toMethod << "Address of return value:" << std::hex << retvalAddr << "." << std::endl;
        break;
    }
    default:
    	break;
    }
}

bool AndroidMonitor::isAppUnderTest(uint32_t pid) {
	return aut_started ? pid == aut.process.pid : false;
}

void AndroidMonitor::onProcessFork(S2EExecutionState *state, uint32_t clone_flags, linux_task* task) {
	//	s2e()->getDebugStream() << "AndroidMonitor: Fork details: " << "Clone Flags: " << hex << clone_flags << endl << LinuxMonitor::dumpTask(task) << endl;
	linux_task appthread = *task;
	if (!isAppUnderTest(appthread.tgid)) {
		return;
	}
	if (appthread.tgid == appthread.pid) {
		//we skip the detection of main processes of new apps, because this is done from inside Dalvik in a more reliable way
		return;
	}

#if 0
	//XXX: Delete this if not needed
	if (clone_flags == 0x11) {
				s2e()->getDebugStream() << "AndroidMonitor:: Main process of unknown Android application started... Is it the one we want to analyze? (wait for a sign from the OS)" << endl;
				assert(appthread.pid == appthread.tgid);

	} else if (clone_flags == 0x450F00) {
				//typical clone_flag of a new thread.
	}
#endif


	if(!task->comm.compare("zygote")) {
			s2e()->getDebugStream() << "AndroidMonitor:: New thread "<< appthread.pid << " of target application "<< aut.process.comm << "( "<< aut.process.pid << " ) detected" << endl;
			aut.threads.insert(appthread);

	} else if (!task->comm.compare("Binder Thread #")) {
		s2e()->getDebugStream() << "Binder Thread of Android application started." << endl;
		aut.threads.insert(appthread);
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
	 *	2. Additional 4-6 threads (clone_flags = CLONE_VFORK & CLONE_FILES & CLONE_VM & CSIGNAL) are created. c.f., dalvik/vm/Init.c:dvmInitAfterZygote(..)
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
		s2e()->getDebugStream() << "AndroidMonitor:: system_server thread discovered with pid: " << dec << thread->pid << endl;
		linux_task newThread = *process;
		systemServer.threads.insert(newThread);
		return;
	}
	if (isAppUnderTest(thread->tgid)) {
		s2e()->getDebugStream() << "AndroidMonitor:: application thread discovered with pid: " << dec << thread->pid << endl;
		linux_task newThread = *process;
		aut.threads.insert(newThread);
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

    if (!swivec_connected) {

		FunctionMonitor::CallSignal *callSignal;

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

	if (!atomic_connected) {
		FunctionMonitor::CallSignal *atomic_cmpxchgSignal; //for debugging purposed
		atomic_cmpxchgSignal = m_funcMonitor->getCallSignal(state, 0xafd0eac4, -1);
		atomic_cmpxchgSignal->connect(sigc::mem_fun(*this, &AndroidMonitor::cmpxchgHook));
		atomic_connected = true;
	}

}

void AndroidMonitor::swiHook(S2EExecutionState* state, FunctionMonitorState *fns) {

	//Test: We skip SWI's when something is symbolic
	uint64_t smask = state->getSymbolicRegistersMask();
	if (smask) {
		state->dumpCpuState(s2e()->getDebugStream());
		uint32_t lr;
		state->readCpuRegisterConcrete(offsetof(CPUState, regs[14]), &lr, 4);
		s2e()->getDebugStream() << "AndroidMonitor:: SWI from " << hex << lr << dec << ". SMASK:" << std::hex << smask << endl;
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

void AndroidMonitor::cmpxchgHook(S2EExecutionState* state, FunctionMonitorState *fns) {
	uint64_t smask = state->getSymbolicRegistersMask();
	if (smask) {
		state->dumpCpuState(s2e()->getDebugStream());
		s2e()->getDebugStream() << "AndroidMonitor:: libc.atomic_cmpxchg with non-empty symbregmask called. SMASK:" << std::hex << smask << endl;
//		if(!m_tbtracer) {
//			m_tbtracer = static_cast<TranslationBlockTracer*>(s2e()->getPlugin("TranslationBlockTracer"));
//			assert(m_tbtracer);
//			m_tbtracer->enableInstructionTracing();
//		}
	}
}


} // namespace plugins
} // namespace s2e
