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

#ifndef _LINUXMONITOR_PLUGIN_H_

#define _LINUXMONITOR_PLUGIN_H_

#include <s2e/Plugins/ModuleDescriptor.h>

#include <s2e/Plugin.h>
#include <s2e/Plugins/CorePlugin.h>
#include <s2e/Plugins/OSMonitor.h>
#include <s2e/Plugins/Linux/LinuxStructures.h>
#include <s2e/Plugins/Android/AndroidMonitor.h>
#include <linux/types.h>
#include <vector>
#include <sstream>
#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <map>

#define KERNEL_START 	0xc0000000

namespace s2e {
namespace plugins {

class AndroidMonitor; //forward declaration

class LinuxMonitor: public OSMonitor {
	S2E_PLUGIN

public:
	friend class AndroidMonitor;

	typedef std::set<std::string> StringSet;
	typedef s2e::linuxos::vm_area kernel_function_area;
	typedef std::map<std::string,s2e::linuxos::symbol_struct> SymbolTable;
private:

	//kernel_symbols which are currently used
	struct kernel_symbols {
		uint32_t do_execve;
		uint32_t start_thread;
		uint32_t init_task;
		uint32_t switch_to;			/* detect task switching */
		uint32_t sys_syscall;		/* trace syscalls */

		uint32_t do_fork; 			/* Android applications are forked from Zygote
									 * so tracing forks is useful
									 */

		uint32_t qemu_trace_fork;   /* needs kernel option CONFIG_QEMU_TRACE enabled
									 * makes it easier to retrieve task_struct
									 */

		uint32_t do_exit;			/* is called whenever a process exits */
		uint32_t sys_exit;			/* syscall handler for process exit */
		uint32_t sys_exit_group;	/* called when a whole group exits */
		uint32_t __irq_usr;
	};


	// contains offsets within Linux kernel data structures
	struct kernel_offsets {
		uint32_t task_comm;
		uint32_t task_pid;
		uint32_t task_tgid;
		uint32_t task_mm;
		uint32_t task_next;
		uint32_t thread_info_task;
		uint32_t mm_code_start;
		uint32_t mm_code_end;
		uint32_t mm_data_start;
		uint32_t mm_data_end;
		uint32_t mm_heap_start;
		uint32_t mm_heap_end;
		uint32_t mm_stack_start;
		uint32_t vmarea_start;
		uint32_t vmarea_end;
		uint32_t vmarea_next;
		// XXX: |==> The following 3 offsets are ugly. They are needed to get vmarea library names with the following approach:
		uint32_t vmarea_file; //  |==>    1. get struct 'file' in 'vm_area_struct' (offset.vmarea_file)
		uint32_t file_dentry; //  |==>    2. get struct 'dentry' in 'file.path' (offset.file_dentry = offsetof(file,path)   + offsetof(path,dentry)
		uint32_t dentry_name; //  |==>    3. get string 'name' in 'dentry.qstr' (offset.dentry_name = offsetof(dentry,qstr) + offsetof(qstr,name)

	};

	// contains currently used Linux kernel symbols (mostly imported from System.map)
	struct kernel_symbols symbols;
	// contains currently used offsets of various structs
	struct kernel_offsets offsets;

	uint32_t threadsize; //needed to find current pid (default: 8192)
	uint32_t last_pid;

	s2e::linuxos::TaskSet tasks; //all discovered tasks during scheduler switch
	std::map<uint32_t,TaskSet> threadmap; //threads assigned to the pid (tgid) of a process

	StringSet libs; //names of prelinked libraries (libraries which do not relocate)
	SymbolTable symboltable; //stores all kernel addresses of System.map

	bool found_kernel_area;
	bool found_libsys_area;
	bool found_libapp_area;

	//rough borders in Android systems (retrieved from prelink-linux-<arch>.map)
	s2e::linuxos::vm_area *kernel_area; //region where kernel code lives
	s2e::linuxos::vm_area *libsys_area; //region where prelinked system libraries live
	s2e::linuxos::vm_area *libapp_area; //region where prelinked app libraries live

	//booleans to hold if we already observe a specific instruction execution
    bool doExecVeConnected;
    bool startThreadConnected;
    bool syscallConnected;
    bool doForkconnected;
    bool qemuTraceForkconnected;
    bool doExitConnected;
    bool switchToConnected;
    bool doIrqUsr;

	sigc::connection onTranslateInstructionConnection;


	S2EExecutionState *currentState;

	// specifies whether VM areas belonging to a task shall be tracked like and treated as modules
	bool track_vm_areas;

	// specifies whether a prelinked map file is specified and prelinked libs should be detected
	bool find_prelinked_libs;

	void notifyModuleLoad(S2EExecutionState *state, s2e::linuxos::linux_task *task);

	uint32_t getCurrentSyscall(S2EExecutionState *state);

	void notifySyscall(S2EExecutionState *state, uint32_t nr_syscall);

#ifdef TARGET_ARM
	//computes the location of struct thread_info. thread_info contains the context of the currently running thread/process, including a pointer to task_struct
	uint32_t getCurrentThreadInfo(S2EExecutionState *state);

	//returns address to task_struct from provided location of thread_info structure.
	uint32_t getTaskStructFromThreadInfo(S2EExecutionState *state, uint32_t thread_info_Address);

	//returns the pid contained in the provided the task_struct
	uint32_t getPidFromTaskStruct(S2EExecutionState *state, uint32_t task_struct_Address);

	uint32_t getCurrentPidFromKernel(S2EExecutionState *state);

#endif

	void handleTaskTransition(S2EExecutionState *state, uint32_t pc, linux_task prev, linux_task next);
	bool registerNewProcess(S2EExecutionState *state, linux_task &process);
	bool registerNewThread(S2EExecutionState *state, linux_task &thread);

	void compilePattern(const char *pattern, regex_t *result);
	bool parseSystemMapFile(const char *system_map_file, SymbolTable &result);
	void parsePrelinkFile(const char *prelink_linux_map_file);
	bool findPrelinkedLibsInFile(std::ifstream &filestream, regex_t *compiled_pattern, StringSet &libs);
	bool findAreaInFile(std::ifstream &filestream, regex_t *compiled_pattern, vm_area *result);

	// search for a task by it's name (the 'comm' field in the task_struct)
	// task is an output parameter the task information will be written to
	bool searchForTask(S2EExecutionState *state, std::string name, s2e::linuxos::linux_task *task);

	//search for a task by pid. if findProcess is true, it aims to find
	// the corresponding process of task with the given pid
	bool searchForTask(S2EExecutionState *state, uint32_t pid, bool findProcess, linux_task *result);


public:
	LinuxMonitor(S2E* s2e) :
		OSMonitor(s2e) {
	}
	virtual ~LinuxMonitor();
	void initialize();

	virtual bool getImports(S2EExecutionState *s, const ModuleDescriptor &desc,
			Imports &I);
	virtual bool getExports(S2EExecutionState *s, const ModuleDescriptor &desc,
			Exports &E);
	virtual bool isKernelAddress(uint64_t pc) const;
	virtual uint64_t getPid(S2EExecutionState *s, uint64_t pc);

	/*
	 * searches the kernel symbol table retrieved from System.map
	 * returns false if not found
	 */
	bool searchSymbol(std::string symbolname, symbol_struct &result);

	/*
	 * retrieves task with given pid
	 * only searches in the set of registered tasks, see field: TaskSet tasks;
	 */
	bool getTask(uint32_t pid, s2e::linuxos::linux_task *task);

	// creates a new task object based on the supplied task_struct memory address
	// this includes: comm, pid, mm and the pointer to the next task_struct
	linux_task getTaskInfo(uint32_t address);

	// fills the given task struct with information on code and data memory areas
	// from the task's mm_struct
	void getTaskMemory(s2e::linuxos::linux_task *task);

	// fills the given task struct with information on all it's VM areas
	void getTaskVMareas(s2e::linuxos::linux_task *task);

	// returns a string containing all available information on the task
	static std::string dumpTask(s2e::linuxos::linux_task *task);

	static std::string dumpContextSwitch(s2e::linuxos::linux_task *prev_task, s2e::linuxos::linux_task *next_task);

	void onTranslateInstruction(ExecutionSignal *signal,
			S2EExecutionState *state, TranslationBlock *tb, uint64_t pc);
	void onInstructionExecution(S2EExecutionState *state, uint64_t pc);

	// **** available signals ***

	sigc::signal<void, S2EExecutionState*,
	uint32_t, /* clone_flags */
	s2e::linuxos::linux_task* /* currently created task */
	> onProcessFork;

	sigc::signal<void, S2EExecutionState*, uint32_t /* syscall number */
	> onSyscall;

	sigc::signal<void, S2EExecutionState*, s2e::linuxos::linux_task* /* new process */
	> onNewProcess;

	sigc::signal<void, S2EExecutionState*, s2e::linuxos::linux_task*, /* new thread */
	s2e::linuxos::linux_task* /* corresponding process */
	> onNewThread;

}; //class LinuxMonitor

} // namespace plugins
} // namespace s2e

#endif
