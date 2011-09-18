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

/**
 *  This plugin automagically detects process load in a Linux Kernel
 *  It is modified by Andreas Kirchner <akalypse@gmail.com> to suit special needs for Android emulator.
 *
 */

extern "C" {
#include "config.h"
#include "qemu-common.h"
#include <regex.h>
}


#include <s2e/S2E.h>
#include <s2e/Utils.h>
#include <s2e/ConfigFile.h>
#include "LinuxMonitor.h"

#include <sstream>
#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <map>

#define READCONFIG_INT(C) C = s2e()->getConfig()->getInt(getConfigKey() + "." + #C)
#define IMPORT_SYMBOL(NAME,RESULT)																			\
																											\
		if (!searchSymbol(NAME,temp)) {																	\
			s2e()->getWarningsStream() << "Symbol " << #NAME << " not found in System.map." << std::endl;	\
			exit(1);																						\
		}																									\
		RESULT = temp.adr;


using namespace std;

using namespace s2e;
using namespace s2e::plugins;
using namespace s2e::linuxos;

S2E_DEFINE_PLUGIN(LinuxMonitor, "Plugin for monitoring Linux events", "Interceptor");

LinuxMonitor::~LinuxMonitor() {

	free(kernel_area);
	free(libsys_area);
	free(libapp_area);

}

void LinuxMonitor::initialize() {

	track_vm_areas = s2e()->getConfig()->getBool(getConfigKey() + ".track_vm_areas", false);
    doExecVeConnected = false;
    startThreadConnected = false;
    syscallConnected = false;
    doForkconnected = false;
    qemuTraceForkconnected = false;
    doExitConnected = false;
    switchToConnected = false;
    
    // read important kernel symbol offsets from config
    // TODO: This has to disappear (automatically retrieve from vmlinux)
    READCONFIG_INT(offsets.task_comm);
    READCONFIG_INT(offsets.task_pid);
    READCONFIG_INT(offsets.task_tgid);
    READCONFIG_INT(offsets.task_mm);
    READCONFIG_INT(offsets.task_next);			//ARM: tasks
    READCONFIG_INT(offsets.thread_info_task);	//ARM: offset to task_struct in thread_info (for context switch)
    READCONFIG_INT(offsets.mm_code_start);		//ARM: start_code
    READCONFIG_INT(offsets.mm_code_end);		//ARM: end_code
    READCONFIG_INT(offsets.mm_data_start);		//ARM: start_data
    READCONFIG_INT(offsets.mm_data_end); 		//ARM: end_data
    READCONFIG_INT(offsets.mm_heap_start); 		//ARM: start_brk
    READCONFIG_INT(offsets.mm_heap_end);		//ARM: brk
    READCONFIG_INT(offsets.mm_stack_start);		//ARM: start_stack
    READCONFIG_INT(offsets.vmarea_start);
    READCONFIG_INT(offsets.vmarea_end);
    READCONFIG_INT(offsets.vmarea_next);
    READCONFIG_INT(offsets.vmarea_file);
    READCONFIG_INT(offsets.file_dentry);
    READCONFIG_INT(offsets.dentry_name);
    READCONFIG_INT(threadsize);

    track_vm_areas = s2e()->getConfig()->getBool(getConfigKey() + ".track_vm_areas", false);

    //parse the system.map file
    bool ok = false;
    const char *system_map_file = s2e()->getConfig()->getString(getConfigKey() + ".system_map_file", "", &ok).c_str();
    if (!ok) {
        s2e()->getWarningsStream() << "No System.map file provided. System.map is needed for Linux Monitor to work properly. Quit." << std::endl;
	    exit(1);   // call system to stop
    }

    if (!parseSystemMapFile(system_map_file,symboltable)) {
    		exit(1);
    }


    // read important kernel symbols from System.map
	symbol_struct temp;
    IMPORT_SYMBOL("do_execve",symbols.do_execve);
    IMPORT_SYMBOL("init_task",symbols.init_task);
#ifdef TARGET_ARM
    IMPORT_SYMBOL("__switch_to",symbols.switch_to);
#endif
    IMPORT_SYMBOL("sys_syscall",symbols.sys_syscall);
    IMPORT_SYMBOL("do_fork",symbols.do_fork);
    IMPORT_SYMBOL("qemu_trace_fork",symbols.qemu_trace_fork);
    IMPORT_SYMBOL("do_exit",symbols.do_exit);
    IMPORT_SYMBOL("sys_exit",symbols.sys_exit);
    IMPORT_SYMBOL("sys_exit_group", symbols.sys_exit_group);
    IMPORT_SYMBOL("__irq_usr", symbols.__irq_usr);

    /* in some architectures (e.g. ARM) start_thread is
     * only a macro. Lets use the address right after the
     * branch to search_binary-Hanlder in do_execve for ARM for now.
     * We don't find this in System.map, so have to read it from config.file
     */
    READCONFIG_INT(symbols.start_thread);

    //should we parse the prelinked libraries file? (Android specific)
    ok = false;
    const char *prelink_linux_map_file = s2e()->getConfig()->getString(getConfigKey() + ".prelink_file", "", &ok).c_str();
    if (!ok) {
    	find_prelinked_libs = false;
        s2e()->getWarningsStream() << "No prelink_file argument. Disable detecting prelinked libraries." << std::endl;
    } else {
    	find_prelinked_libs = true;
    	parsePrelinkFile(prelink_linux_map_file);
    }




    // **** signal connecting ***
    onTranslateInstructionConnection = s2e()->getCorePlugin()->onTranslateInstructionStart.connect(sigc::mem_fun(*this, &LinuxMonitor::onTranslateInstruction));


}

bool LinuxMonitor::searchSymbol(std::string name, symbol_struct &result) {
	SymbolTable::iterator it;
	it = symboltable.find(name);

	if (it == symboltable.end()) {
		return false;
	}

	result = it->second;
	return true;

}

bool LinuxMonitor::parseSystemMapFile(const char *system_map_file, SymbolTable &result) {
	parse_expr pattern = {"symbol_entry", "(^[[:xdigit:]]{8,8}) (.) (.*)$", NULL };

	//open file
	ifstream system_map_stream;
	system_map_stream.open(system_map_file);
	if (!system_map_stream) {
	    s2e()->getWarningsStream() << "LinuxMonitor:: Unable to open System.map file" << system_map_file << "." << endl;
	    exit(1);   // call system to stop
	}

	 pattern.compiled_pattern = (regex_t*)malloc(sizeof(regex_t));
	 compilePattern(pattern.pattern,pattern.compiled_pattern);

	char line[255];
	size_t nmatch = 4;
	regmatch_t matchptr [nmatch];
	int regret;

	 while (system_map_stream) {
		 system_map_stream.getline(line,255);
	 		regret = regexec(pattern.compiled_pattern, line, nmatch, matchptr, 0);
	 		if (0 == regret) {
	 			//match, get the subexpressions
	 			size_t adr_len = matchptr[1].rm_eo - matchptr[1].rm_so;
	 			size_t type_len = matchptr[2].rm_eo - matchptr[2].rm_so;
	 			size_t name_len = matchptr[3].rm_eo - matchptr[3].rm_so;

	 			char *s_adr = new char[adr_len+1];
	 			char * s_type = new char[type_len+1];
	 			char *s_name = new char[name_len+1];

	 			strncpy(s_adr,(line+matchptr[1].rm_so),adr_len);
	 			strncpy(s_type,(line+matchptr[2].rm_so),type_len);
	 			strncpy(s_name,(line+matchptr[3].rm_so),name_len);

	 			s_adr[adr_len]='\0';
	 			s_type[type_len]='\0';
	 			s_name[name_len]='\0';

	 			symbol_struct sym;
	 			sym.adr = (uint32_t) strtoul(s_adr,NULL,16);
	 			sym.type = s_type[0];
	 			std::string name = std::string(s_name);
	 			sym.name = name;

	 			result[name]=sym; //insert

	 			delete[] s_adr;
	 			delete[] s_type;
	 			delete[] s_name;

	 		} else if (REG_NOMATCH == regret){
	 			continue;
	 		} else {
	 			size_t length = regerror (regret, pattern.compiled_pattern, NULL, 0);
	 			char *buffer = (char*) malloc(length);
	 			(void) regerror (regret, pattern.compiled_pattern, buffer, length);
	 			s2e()->getWarningsStream() << "LinuxMonitor::parseSystemMap: Error matching regex. msg: "<< buffer << "." << endl;
	 			return false;
	 		}
	 	}

	 regfree (pattern.compiled_pattern);
	 free(pattern.compiled_pattern);

	s2e()->getMessagesStream() << "LinuxMonitor:: successfully parsed "<< dec << symboltable.size() << " symbols from System.map"<< endl;

	 system_map_stream.close();
	 return true;
}

void LinuxMonitor::parsePrelinkFile(const char *prelink_linux_map_file) {

	// a collection of patterns to parse the prelink-linux-<arch>.map
	static parse_expr region_patterns[] = {
			{"kernel", "(0x[[:xdigit:]]{8,8}) - (0x[[:xdigit:]]{8,8}) (Kernel)", NULL},									// kernel region
			{"prelinked_system_libs", "(0x[[:xdigit:]]{8,8}) - (0x[[:xdigit:]]{8,8}) (Prelinked System.*)", NULL},		// address region for prelinked system libraries
			{"prelinked_app_libs", "(0x[[:xdigit:]]{8,8}) - (0x[[:xdigit:]]{8,8}) (Prelinked App.*)", NULL},			// address region for prelinked application libraries
			{"prelinked_single_lib", "([[:alnum:]]+.so)[[:space:]]+(0x[[:xdigit:]]{8,8})", NULL}						// name + start address of a single prelinked library
	};

	//open prelink_file
	ifstream prelink_map_stream;
	prelink_map_stream.open(prelink_linux_map_file);
	if (!prelink_map_stream) {
	    s2e()->getWarningsStream() << "LinuxMonitor:: Unable to open prelink_file" << prelink_linux_map_file << "." << endl;
	    exit(1);   // call system to stop
	}

	//first compile all expressions
	unsigned int i;
	unsigned int num_patterns = sizeof( region_patterns ) / sizeof( region_patterns[0] );
	for(i=0; i < (num_patterns); i++) {
		region_patterns[i].compiled_pattern = (regex_t*)malloc(sizeof(regex_t));
		compilePattern(region_patterns[i].pattern, region_patterns[i].compiled_pattern);
	}

	found_kernel_area = false;
	found_libsys_area = false;
	found_libapp_area = false;

	kernel_area = new vm_area;
	libsys_area = new vm_area;
	libapp_area = new vm_area;

	//parse the file
	if (findAreaInFile(prelink_map_stream,region_patterns[0].compiled_pattern,kernel_area)) {
		s2e()->getMessagesStream() << "LinuxMonitor:: parsed kernel area: 0x" << hex << kernel_area->start << " - 0x" << hex << kernel_area->end << endl;
    	found_kernel_area = true;
	}
	if (findAreaInFile(prelink_map_stream,region_patterns[1].compiled_pattern,libsys_area)) {
		s2e()->getMessagesStream() << "LinuxMonitor:: parsed prelinked system libraries area: 0x" << hex << libsys_area->start << " - 0x" << hex << libsys_area->end << endl;
    	found_libsys_area = true;
	}
	if (findAreaInFile(prelink_map_stream,region_patterns[2].compiled_pattern,libapp_area)) {
		s2e()->getMessagesStream() << "LinuxMonitor:: parsed prelinked application libraries area: 0x" << hex << libapp_area->start << " - 0x" << hex << libapp_area->end << endl;
    	found_libapp_area = true;
	}

	if (findPrelinkedLibsInFile(prelink_map_stream, region_patterns[3].compiled_pattern, libs)) {
		s2e()->getMessagesStream() << "LinuxMonitor:: successfully parsed "<< dec << libs.size() << " pre-linked libraries"<< endl;
	}

	for(i=0; i < (num_patterns); i++) {
		 regfree (region_patterns[i].compiled_pattern);
		 free(region_patterns[i].compiled_pattern);
	 }

	prelink_map_stream.close();
}


void LinuxMonitor::compilePattern(const char *pattern, regex_t *result) {
	int regret;
	regret = regcomp(result,pattern,REG_ICASE|REG_EXTENDED);
	if (regret != 0) {
		  size_t length = regerror (regret, result, NULL, 0);
		  char *buffer = (char*) malloc(length);
		  (void) regerror (regret, result, buffer, length);
		  s2e()->getWarningsStream() << "LinuxMonitor:: Error compiling regex " << pattern << " msg: "<< buffer << "." << endl;
		  exit(1);   // call system to stop
	}
}

/*
 * parse a pre-linux-<arch>.map file to find an adress region
 * return false if unsuccessful.
 * vm_area result contains the region described with the matching pattern
 * filestream has to be opened
 * stops parsing the file after the first match
 */
bool LinuxMonitor::findAreaInFile(ifstream &filestream, regex_t *compiled_pattern, vm_area *result) {

	if (!filestream.is_open()){
		s2e()->getWarningsStream() << "LinuxMonitor::findAreaInFile: Filestream not opened." << endl;
		return false;
	}

	//first reset to beginning
	filestream.clear() ;
	filestream.seekg(0, ios::beg);

	char line[255];
	size_t nmatch = 4;
	regmatch_t matchptr [nmatch];
	int regret;
	while (filestream) {
		filestream.getline(line,255);
		regret = regexec(compiled_pattern, line, nmatch, matchptr, 0);
		if (0 == regret) {
			//match, get the subexpressions
			size_t region_start_len = matchptr[1].rm_eo - matchptr[1].rm_so;
			size_t region_end_len = matchptr[2].rm_eo - matchptr[2].rm_so;
			size_t region_name_len = matchptr[3].rm_eo - matchptr[3].rm_so;

			char *s_region_start = new char[region_start_len+1];
			char *s_region_end = new char[region_end_len+1];
			char *s_region_name = new char[region_name_len+1];

			strncpy(s_region_start,(line+matchptr[1].rm_so),region_start_len);
			strncpy(s_region_end,(line+matchptr[2].rm_so),region_end_len);
			strncpy(s_region_name,(line+matchptr[3].rm_so),region_name_len);
			s_region_start[region_start_len]='\0';
			s_region_end[region_end_len]='\0';
			s_region_name[region_name_len]='\0';

			result->start = (uint32_t) strtoul(s_region_start,NULL,0);
			result->end = (uint32_t) strtoul(s_region_end,NULL,0);
			std::string name = std::string(s_region_name);
			result->name = name;

			delete[] s_region_start;
			delete[] s_region_end;
			delete[] s_region_name;

			return true;

		} else if (REG_NOMATCH == regret){
			continue;
		} else {
			size_t length = regerror (regret, compiled_pattern, NULL, 0);
			char *buffer = (char*) malloc(length);
			(void) regerror (regret, compiled_pattern, buffer, length);
			s2e()->getWarningsStream() << "LinuxMonitor::findAreaInFile: Error matching regex. msg: "<< buffer << "." << endl;
			return false;
		}
	}

	return false;
}

bool LinuxMonitor::findPrelinkedLibsInFile(ifstream &filestream, regex_t *compiled_pattern, StringSet &libs) {

	if (!filestream.is_open()){
		s2e()->getWarningsStream() << "LinuxMonitor::findPrelinkedLibsInFile: Filestream not opened." << endl;
		return false;
	}

	//first reset to beginning
	filestream.clear() ;
	filestream.seekg(0, ios::beg);

	char line[255];
	size_t nmatch = 3;
	regmatch_t matchptr [nmatch];
	int regret;

	while (filestream) {
		filestream.getline(line,255);
		regret = regexec(compiled_pattern, line, nmatch, matchptr, 0);
		if (0 == regret) {
			//match, get the subexpressions
			size_t name_len = matchptr[1].rm_eo - matchptr[1].rm_so;
			size_t startaddr_len = matchptr[2].rm_eo - matchptr[2].rm_so;

			char *s_name = (char*)malloc(name_len+1);
			char *s_startaddr = (char*)malloc(startaddr_len+1);

			strncpy(s_name,(line+matchptr[1].rm_so),name_len);
			strncpy(s_startaddr,(line+matchptr[2].rm_so),startaddr_len);
			s_name[name_len]='\0';
			s_startaddr[startaddr_len]='\0';

			std::string name = std::string(s_name);
			//we dont use the address for now
			//uint32_t addr = (uint32_t) strtoul(s_startaddr,NULL,0);
			libs.insert(name);

			delete[] s_name;
			delete[] s_startaddr;

		} else if (REG_NOMATCH == regret){
			continue;
		} else {
			  size_t length = regerror (regret, compiled_pattern, NULL, 0);
			  char *buffer = (char*) malloc(length);
			  (void) regerror (regret, compiled_pattern, buffer, length);
			  s2e()->getWarningsStream() << "LinuxMonitor::findAreaInFile: Error matching regex. msg: "<< buffer << "." << endl;
		}
	} // while
	return true;
}

void LinuxMonitor::onTranslateInstruction(ExecutionSignal *signal, S2EExecutionState *state, TranslationBlock *tb, uint64_t pc) {

    
    if(pc == symbols.do_execve && !doExecVeConnected) {
        signal->connect(sigc::mem_fun(*this, &LinuxMonitor::onInstructionExecution));
        doExecVeConnected = true;
    }
    if(pc == symbols.start_thread && !startThreadConnected) {
        signal->connect(sigc::mem_fun(*this, &LinuxMonitor::onInstructionExecution));
        startThreadConnected = true;
    }
    
    if(pc == symbols.sys_syscall && !syscallConnected) {
    	signal->connect(sigc::mem_fun(*this, &LinuxMonitor::onInstructionExecution));
    	syscallConnected = true;
    }

    if(pc == symbols.do_fork && !doForkconnected) {
    	signal->connect(sigc::mem_fun(*this, &LinuxMonitor::onInstructionExecution));
    	doForkconnected = true;
    }

    if(pc == symbols.qemu_trace_fork && !qemuTraceForkconnected) {
    	signal->connect(sigc::mem_fun(*this, &LinuxMonitor::onInstructionExecution));
    	qemuTraceForkconnected = true;
    }

    if( pc == symbols.do_exit && !doExitConnected) {
    	signal->connect(sigc::mem_fun(*this, &LinuxMonitor::onInstructionExecution));
    	doExitConnected = true;
    }

    if( pc == symbols.__irq_usr && !doIrqUsr) {
    	signal->connect(sigc::mem_fun(*this, &LinuxMonitor::onInstructionExecution));
    	doIrqUsr = true;
    }

#ifdef TARGET_ARM
    if (pc == symbols.switch_to && !switchToConnected) {
    	signal->connect(sigc::mem_fun(*this, &LinuxMonitor::onInstructionExecution));
    	switchToConnected = true;
    }
#endif

    if (doExecVeConnected && startThreadConnected)
        onTranslateInstructionConnection.disconnect();

} // onTranslateInstruction

void LinuxMonitor::onInstructionExecution(S2EExecutionState *state, uint64_t pc) {
    // this vector stores processes for which do_execve has been called but whose task_struct has not yet be found in memory
    static vector<string> pendingLookups;
    
    currentState = state;

    if (pc == symbols.do_execve) {
		uint32_t fnAddress;
		state->readCpuRegisterConcrete(offsetof(CPUState, regs[0]), &fnAddress, 4);
		string fn;
		state->readString(fnAddress, fn, 100);
		s2e()->getDebugStream() << "LinuxMonitor: do_execve called for task " << fn << endl;
		size_t pos = fn.find_last_of('/');
		fn = fn.substr(pos + 1);
		pendingLookups.push_back(fn);
		goto end;
    }
	/*
	 *  During the startup process of an elf binary, the last kernel method
	 *  invoked is start_thread. It calls the entry point of the binary.
	 *  Hence we use it to search the memory for the recently loaded elf binary.
	 */
    if (pc == symbols.start_thread) {
		vector<string>::iterator it = pendingLookups.begin();
		while (it != pendingLookups.end()) {
			linux_task task;
			if (searchForTask(state, *it, &task)) {
				s2e()->getDebugStream() << "LinuxMonitor: task loaded: " << task.comm << endl;
				notifyModuleLoad(state, &task);
				pendingLookups.erase(it);
			} else {
				it++;
			}
		}
		goto end;
    }

	if (pc == symbols.sys_syscall) {
		uint32_t nr_syscall = getCurrentSyscall(state);
		notifySyscall(state,nr_syscall);
		goto end;
	}

//	if (pc == symbols.do_fork) {
//		//XXX: Do we need this?
//    	goto end;
//	}

	if (pc == symbols.qemu_trace_fork) {
		linux_task task;
		uint32_t task_struct_Address;
		uint32_t clone_flags;
		//get the task_struct
		state->readCpuRegisterConcrete(offsetof(CPUState, regs[0]), &task_struct_Address, 4);
		state->readCpuRegisterConcrete(offsetof(CPUState, regs[1]), &clone_flags, 4);
		task = getTaskInfo(task_struct_Address);

		getTaskMemory(&task);
		if ( track_vm_areas ) {
			getTaskVMareas(&task);
		}
		notifyModuleLoad(state,&task);

		onProcessFork.emit(state,clone_flags,&task);
		goto end;
	}

	if (pc == symbols.do_exit) {
#ifdef TARGET_ARM
    	uint32_t pid = getCurrentPidFromKernel(state);
#elif defined(TARGET_I386)
    	uint32_t pid = getPid(state,pc);
#endif
    	linux_task task;
    	if (getTask(pid,&task)) {
        	s2e()->getDebugStream() << "LinuxMonitor: Task " << task.comm << " (pid: " << dec << task.pid << ") ends." << endl;
    		tasks.erase(task);
        	std::map<uint32_t,TaskSet>::iterator element = threadmap.find(pid);
        	if (element != threadmap.end()) {
        		s2e()->getDebugStream() << "Corresponding threads are also unloaded:" << endl;
        		TaskSet::iterator threadIt;
        		//XXX: Usually, this should happen anyway, because do_exit is called for each task.
        		for ( threadIt=element->second.begin() ; threadIt != element->second.end(); threadIt++ ) {
        			s2e()->getDebugStream() << "Thread: " << threadIt->comm << " (pid: " << threadIt->pid << " ) ends." << endl;
        			onProcessUnload.emit(state,threadIt->pid);
        			tasks.erase(*threadIt);
        		}
        		threadmap.erase(task.pid);
        	}
    	}
    	onProcessUnload.emit(state,pid);
    	goto end;
	}

#ifdef TARGET_ARM
    if (pc == symbols.switch_to) {
        uint32_t next_thread_info_Address;
        uint32_t next_task_struct_Address;
        uint32_t previous_task_struct_Address;

        // r0 = previous task_struct, r1 = previous thread_info, r2 = next thread_info
        state->readCpuRegisterConcrete(offsetof(CPUState, regs[0]), &previous_task_struct_Address, 4);

        state->readCpuRegisterConcrete(offsetof(CPUState, regs[2]), &next_thread_info_Address, 4);
        next_task_struct_Address = getTaskStructFromThreadInfo(state,next_thread_info_Address);

        linux_task prevTask = getTaskInfo(previous_task_struct_Address);
        linux_task nextTask= getTaskInfo(next_task_struct_Address);

        handleTaskTransition(state, pc, prevTask, nextTask);

        goto end;
    }
#endif

    if (pc == symbols.__irq_usr) {
    	//tell me when an interrupt occurs
//    	s2e()->getDebugStream() << "interrupt" << endl;
    }
end:
  currentState = NULL;

} //onInstructionExecution

void LinuxMonitor::handleTaskTransition(S2EExecutionState *state, uint32_t pc, linux_task prevTask, linux_task nextTask) {
    last_pid = nextTask.pid; //update pid

    if( tasks.find(nextTask) == tasks.end()) { //not found in list of active tasks

    	//get more informations
        getTaskMemory(&nextTask);
        if ( track_vm_areas ) {
        	getTaskVMareas(&nextTask);
        }

#if 0
    	s2e()->getDebugStream() << "LinuxMonitor: new process with pid " << dec << nextTask.pid << " (tgid: " << nextTask.tgid << ") discovered: " << nextTask.comm << endl;
#endif

    	if (nextTask.pid == nextTask.tgid) {
    		registerNewProcess(state, nextTask);
    	} else  {
    		registerNewThread(state, nextTask);
    	}

        notifyModuleLoad(state,&nextTask);
    }


#if 0
    string out = dumpContextSwitch(&prevTask,&nextTask);
    s2e()->getDebugStream() << "LinuxMonitor:: report context switch" << endl << out << endl;
#endif
}

bool LinuxMonitor::registerNewProcess(S2EExecutionState *state, linux_task &process) {
	//check: is it a process?
	if (process.pid != process.tgid) {
		s2e()->getDebugStream() << "LinuxMonitor::registerNewProcess(): Task " << process.pid << "is not a process, but a thread which belongs to process " << process.tgid << endl;
		return false;
	}

	//first add it to tasklist
	tasks.insert(process);

	if ( threadmap.find(process.pid) != threadmap.end()) { //already registered
		return false;
	}

	//then add it to threadmap with zero attached threads
	TaskSet threads; //empty threadlist
	threadmap.insert(pair<uint32_t,TaskSet>(process.pid,threads));

	s2e()->getDebugStream() << "LinuxMonitor::registerNewProcess(): Process " << process.comm << " (pid: " << dec << process.pid << ") registered." << endl;

	onNewProcess.emit(state,&process);

	return true;

}

bool LinuxMonitor::registerNewThread(S2EExecutionState *state, linux_task &thread) {

	linux_task process;
	if (threadmap.find(thread.tgid) == threadmap.end()) {
		//try to find and register the corresponding process
		if (!searchForTask(state, thread.tgid, true, &process)) {
			s2e()->getDebugStream() << "No corresponding process found for thread " << thread.comm << " ( pid: " << dec << thread.pid << "). We were looking for process with pid: " << dec << thread.tgid  << "."<< endl;
			return false;
		}
		registerNewProcess(state, process);
		assert(threadmap.find(thread.tgid) != threadmap.end());
	}
	assert (getTask(thread.tgid,&process));

	//first add it to tasklist
	tasks.insert(thread);

	//then map it to corresponding process
	threadmap[thread.tgid].insert(thread);



	s2e()->getDebugStream() << "LinuxMonitor::registerNewThread(): Thread " << thread.comm << " ( pid: " << dec << thread.pid << ") attached to " << process.comm << " (pid: " << dec << thread.tgid << ")." << endl;


	onNewThread.emit(state,&thread,&process);

	return true;
}

uint32_t LinuxMonitor::getCurrentSyscall(S2EExecutionState *state) {
	uint32_t nr_syscall;
	state->readCpuRegisterConcrete(offsetof(CPUState, regs[7]), &nr_syscall, 4); //when in sys_syscall, scno (alias r7) contains a syscall_address
	return nr_syscall;
}

void LinuxMonitor::notifySyscall(S2EExecutionState *state, uint32_t nr_syscall) {
	onSyscall.emit(state,nr_syscall);
}


#ifdef TARGET_ARM
uint32_t LinuxMonitor::getCurrentThreadInfo(S2EExecutionState *state) {
	uint32_t sp;
	uint32_t thread_info_Address;
	//struct thread_info of current process is always stored at address: sp & ~(THREAD_SIZE - 1)
	state->readCpuRegisterConcrete(offsetof(CPUState, banked_r13[1]), &sp, 4); //banked_r13[1] represents the stack pointer in Supervisor-mode
	thread_info_Address = (sp & ~(threadsize -1));
	return thread_info_Address;
}

uint32_t LinuxMonitor::getTaskStructFromThreadInfo(S2EExecutionState *state, uint32_t thread_info_Address) {
	uint32 task_struct_Address;
	state->readMemoryConcrete( thread_info_Address+offsets.thread_info_task, &task_struct_Address, 4); //dereferencing pointer to next_task_struct
	return task_struct_Address;
}

uint32_t LinuxMonitor::getPidFromTaskStruct(S2EExecutionState *state, uint32_t task_struct_Address) {
	uint32_t pid;
	state->readMemoryConcrete(task_struct_Address + offsets.task_pid, &pid, 4);
	return pid;
}

uint32_t LinuxMonitor::getCurrentPidFromKernel(S2EExecutionState *s) {
    	uint32_t current_thread_info;
    	uint32_t current_task_struct;
    	uint32_t pid;
    	//structure thread_info of the currently running process for ARM linux
        current_thread_info = getCurrentThreadInfo(s);
    	//thread_info contains task_struct of the currently running process
        current_task_struct = getTaskStructFromThreadInfo(s,current_thread_info);
    	//task_struct finally contains the pid which we want
        pid = getPidFromTaskStruct(s,current_task_struct);
        return pid;
}
#endif

void LinuxMonitor::notifyModuleLoad(S2EExecutionState *state, linux_task *task) {
    ModuleDescriptor md;
    md.Name = task->comm;
    md.LoadBase = task->code_start;
    md.NativeBase = task->code_start; //XXX: Retrieve real native base
    md.Size = task->code_end - task->code_start;
    md.Pid = task->pid;
    onModuleLoad.emit(state, md);
    if (track_vm_areas) {
        for (vector<vm_area>::iterator it = task->vm_areas.begin(); it < task->vm_areas.end(); it++) {
            if (it->name.compare(task->comm) != 0) {

					ModuleDescriptor mdl;
					mdl.Name = it->name;
					mdl.LoadBase = it->start;
					mdl.Size = it->end - it->start;
//					if(libs.find(it->name) != libs.end()) { //only assign pid when not pre-defined module.
//						mdl.Pid = 0;
//	                } else {
	                	mdl.Pid = task->pid;
//	                }
					onModuleLoad.emit(state, mdl);

            }
        }
    }
}




bool LinuxMonitor::isKernelAddress(uint64_t pc) const {
	return found_kernel_area ? (pc >= kernel_area->start) && (pc <= kernel_area->end) : (pc >= KERNEL_START);
}

uint64_t LinuxMonitor::getPid(S2EExecutionState *s, uint64_t pc) {
    if (isKernelAddress(pc)) {
        return 0;
    } else {
#ifdef TARGET_ARM
    	//two approaches to get the pid in Linux ARM
    	//the commented method derives the pid from the kernel stack
        //uint32_t alt_pid = getCurrentPidFromKernel(s);

    	//last_pid is updated directly after a context_switch
        return last_pid;
#elif defined(TARGET_I386)
    return s->getPid();
#endif
    }
}

bool LinuxMonitor::getImports(S2EExecutionState *s, const ModuleDescriptor &desc, Imports &I) {
    return false;
}

bool LinuxMonitor::getExports(S2EExecutionState *s, const ModuleDescriptor &desc, Exports &E) {
    return false;
}

linux_task LinuxMonitor::getTaskInfo(uint32_t address) {
    linux_task task;
    currentState->readMemoryConcrete(address + offsets.task_pid, &task.pid, 4);
    currentState->readMemoryConcrete(address + offsets.task_tgid, &task.tgid, 4);
    currentState->readString(address + offsets.task_comm, task.comm, 60);
    currentState->readMemoryConcrete(address + offsets.task_next, &task.next, 4);
    task.next -= offsets.task_next;
    currentState->readMemoryConcrete(address + offsets.task_mm, &task.mm, 4);
    return task;
}

void LinuxMonitor::getTaskMemory(linux_task *task) {
    // read code segment from the task's mm_struct
    currentState->readMemoryConcrete(task->mm + offsets.mm_code_start, &task->code_start, 4);
    currentState->readMemoryConcrete(task->mm + offsets.mm_code_end, &task->code_end, 4);
    // read data segment from the task's mm_struct
    currentState->readMemoryConcrete(task->mm + offsets.mm_data_start, &task->data_start, 4);
    currentState->readMemoryConcrete(task->mm + offsets.mm_data_end, &task->data_end, 4);
}

void LinuxMonitor::getTaskVMareas(linux_task *task) {
    uint32_t vmarea;
    currentState->readMemoryConcrete(task->mm, &vmarea, 4);
    bool ok = true;
    do {
        vm_area area;
        uint32_t tmp;
        // get start and end address of the vm area
        currentState->readMemoryConcrete(vmarea + offsets.vmarea_start, &area.start, 4);
        currentState->readMemoryConcrete(vmarea + offsets.vmarea_end, &area.end, 4);
        // get the filename of the mmap call

        currentState->readMemoryConcrete(vmarea + offsets.vmarea_file, &tmp, 4);
        currentState->readMemoryConcrete(tmp + offsets.file_dentry, &tmp, 4);
        currentState->readMemoryConcrete(tmp + offsets.dentry_name, &tmp, 4);

        currentState->readString(tmp, area.name, 30);
        if (area.name.empty())
            area.name = "unknown";
        task->vm_areas.push_back(area);
        // get next vm area
        ok = currentState->readMemoryConcrete(vmarea + offsets.vmarea_next, &vmarea, 4);
    } while (vmarea != NULL && ok);
}

bool LinuxMonitor::searchForTask(S2EExecutionState *state, string name, linux_task *task) {
	currentState = state;
    *task = getTaskInfo(symbols.init_task);
    // iterate over all tasks (except swapper)
    while (task->next != symbols.init_task) {
        *task = getTaskInfo(task->next);
        if (task->comm.compare(name) == 0) {
            // if we found the desired task, retrieve additional information before returning
            getTaskMemory(task);
            getTaskVMareas(task);
            currentState = NULL;
            return true;
        }
    }
    currentState = NULL;
    return false;
}

/*
 * goes through all
 */
bool LinuxMonitor::searchForTask(S2EExecutionState* state, uint32_t pid, bool findProcess, linux_task *result) {
	currentState = state;
    *result = getTaskInfo(symbols.init_task);
    // iterate over all tasks (except swapper)
    while (result->next != symbols.init_task) {
        *result = getTaskInfo(result->next);
        if ( pid == result->pid ) {
        	if ( findProcess && (pid != result->tgid) ) {
        		//we now know that pid identifies a thread, not a process.
        		//thus: look for the process
        		currentState = NULL;
        		return searchForTask(state, result->tgid, true, result);
        	}
            // if we found the desired task, retrieve additional information before returning
            getTaskMemory(result);
            getTaskVMareas(result);
    		currentState = NULL;
            return true;
        }
    }
	currentState = NULL;
    return false;
}
string LinuxMonitor::dumpTask(linux_task *task) {
    ostringstream oss;
    oss << "Task\n\tpid: " << dec << task->pid << " tgid: " << dec << task->tgid <<endl;
    oss << "comm: " << task->comm << endl;
    oss << "Task\n\ttgid: " << dec << task->tgid << endl;
    oss << "\tcode area: 0x" << hex << task->code_start << " - 0x" << task->code_end << endl;
    oss << "\tdata area: 0x" << hex << task->data_start << " - 0x" << task->data_end << endl;
    for (vector<vm_area>::iterator it = task->vm_areas.begin(); it < task->vm_areas.end(); it++) {
        oss << "\tlibrary: " << it->name << " from 0x" << hex << it->start << " to 0x" << it->end << endl;
    }
    return oss.str();
}

string LinuxMonitor::dumpContextSwitch(linux_task *prev_task, linux_task *next_task) {
    ostringstream oss;
    oss << "Context switch from pid " << dec << prev_task->pid << " to pid " << dec << next_task->pid << endl;
    oss << "### Previous Task" << endl;
    oss << dumpTask(prev_task) << endl;
    oss << "### Next Task" << endl;
    oss << dumpTask(next_task) << endl;
	return oss.str();
}

bool LinuxMonitor::getTask(uint32_t pid, linux_task *task)  {
	TaskSet::iterator it;
	for ( it=tasks.begin() ; it != tasks.end(); it++ ) {
		if (pid == it->pid) {
			*task = *it;
			return true;
		}
	}
	return false;
}

