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

/*
 * Essential structures for Linux operating system
 */

#ifndef _LINUX_STRUCTS_H_
#define _LINUX_STRUCTS_H_

#include <regex.h>

namespace s2e {
namespace linuxos {

struct linux_task; //forward declaration
typedef std::set<linux_task> TaskSet;

struct symbol_struct {
	uint32_t adr;
	char type; //symbol type (T,D,...)
	std::string name;
};

// represents a VM area belonging to a process as it is defined in the kernel's vm_area struct
struct vm_area {
	std::string name;
	uint32_t start;
	uint32_t end;

	friend bool operator<(vm_area const& a, vm_area const& b)
	    {
	        return a.start < b.start;
	    }
};



// a linux task_struct represents a thread/process
struct linux_task {
	uint32_t pid;  /* thread id */
	uint32_t tgid; /* thread group id - getpid() in linux returns the tgid */
	std::string comm;
	uint32_t next; //linked list 'tasks' has 'next' as first entry in the list_struct.
	uint32_t mm;
	uint32_t code_start;
	uint32_t code_end;
	uint32_t data_start;
	uint32_t data_end;
	std::vector<struct vm_area> vm_areas;

	friend bool operator<(linux_task const& a, linux_task const& b)
	    {
	        return a.pid < b.pid;
	    }

	friend bool operator==(linux_task const& a, linux_task const& b)
	    {
	        return a.pid == b.pid && a.comm.compare(b.comm);
	    }
};



struct parse_expr {
	const char *name;
	const char *pattern; //regex to parse a line in a file (currently used for prelink-linux-<arch>.map
	regex_t *compiled_pattern;
};

} //namespace linux
} //namespace s2e
#endif // _LINUX_STRUCTS_H_
