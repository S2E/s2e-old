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

#ifndef _ANDROIDMONITOR_PLUGIN_H_

#define _ANDROIDMONITOR_PLUGIN_H_

#include <s2e/Plugin.h>
#include <s2e/Plugins/CorePlugin.h>
#include <s2e/S2EExecutionState.h>
#include <s2e/Plugins/Android/AndroidStructures.h>
#include <s2e/Plugins/Linux/LinuxMonitor.h>
#include <s2e/Plugins/ModuleDescriptor.h>
#include <s2e/Plugins/ModuleExecutionDetector.h>
#include <s2e/Plugins/FunctionMonitor.h>
#include <s2e/Plugins/ExecutionTracers/TranslationBlockTracer.h>
#include <s2e/Plugins/OSMonitor.h>
#include <vector>

namespace s2e {
namespace plugins {

class LinuxMonitor; //forward declaration

class AndroidMonitor:public Plugin
{
    S2E_PLUGIN
    friend class LinuxMonitor;

private:
    s2e::android::DalvikVM zygote;
    s2e::android::DalvikVM systemServer;

    /* TODO: support more than one apps under test */
    s2e::android::DalvikVM aut; // app under test
    bool aut_started;			// is target app started?

	LinuxMonitor * m_linuxMonitor;
	FunctionMonitor *m_funcMonitor;
	s2e::plugins::ModuleExecutionDetector * m_modulePlugin;
	s2e::plugins::TranslationBlockTracer *m_tbtracer;

	std::string aut_process_name; //exact process name of the application under test (or NULL if no specific app is observed)

	bool swivec_connected; //is function handler to hook SWI execption vector registered?
	bool atomic_connected;

    void onCustomInstruction(S2EExecutionState* state, uint64_t opcode);
    void onModuleTransition(S2EExecutionState* state, const ModuleDescriptor *previousModule, const ModuleDescriptor *nextModule);
    void swiHook(S2EExecutionState* state, FunctionMonitorState *fns);
    void cmpxchgHook(S2EExecutionState* state, FunctionMonitorState *fns);

#if 0
    void onException(S2EExecutionState* state, unsigned excpnr, uint64_t pc);
#endif

    bool isAppUnderTest(uint32_t pid);
public:
    AndroidMonitor(S2E* s2e): Plugin(s2e) {};
    virtual ~AndroidMonitor();
    void initialize();

	//new provided signals;
	sigc::signal <void, S2EExecutionState*, s2e::linuxos::linux_task*> onAppStart;

	void onProcessFork(S2EExecutionState *state, uint32_t clone_flags, s2e::linuxos::linux_task* task);
	void onSyscall(S2EExecutionState *state, uint32_t syscall_nr);
	void onNewThread(S2EExecutionState *state, s2e::linuxos::linux_task* thread, s2e::linuxos::linux_task* process);
	void onNewProcess(S2EExecutionState *state, s2e::linuxos::linux_task* process);
	void slotTranslateBlockStart(ExecutionSignal *signal, S2EExecutionState *state, TranslationBlock *tb, uint64_t pc);


};

} // namespace plugins
} // namespace s2e

#endif
