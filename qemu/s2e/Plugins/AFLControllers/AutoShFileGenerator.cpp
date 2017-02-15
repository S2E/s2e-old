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
#include <qemu-common.h>
#include <cpu-all.h>
#include <exec-all.h>
#include <sysemu.h>
#include <cpus.h>
}

#include "AutoShFileGenerator.h"
#include <iomanip>
#include <cctype>

#include <algorithm>
#include <fstream>
#include <vector>
#include <s2e/S2E.h>
#include <s2e/S2EExecutor.h>
#include <s2e/Utils.h>
#include <s2e/ConfigFile.h>

#include <s2e/Plugin.h>
#include <s2e/s2e_qemu.h>
#include <s2e/S2EExecutionState.h>
#include <s2e/S2ESJLJ.h>
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
#include <llvm/Support/TimeValue.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>

namespace s2e {
namespace plugins {
S2E_DEFINE_PLUGIN(AutoShFileGenerator, "AutoShFileGenerator plugin",
		"AutoShFileGenerator",);

AutoShFileGenerator::AutoShFileGenerator(S2E* s2e) :
		Plugin(s2e)
{
	m_current_command = " ";
	m_randtemplate = "{seed}";
}

AutoShFileGenerator::~AutoShFileGenerator()
{
}
void AutoShFileGenerator::initialize()
{
	bool ok = false;
	m_command_file = s2e()->getConfig()->getString(
			getConfigKey() + ".command_file", "", &ok);
	if (!ok) {
		s2e()->getWarningsStream() << "You must specifiy "
				<< getConfigKey() + ".command_file" << '\n';
		exit(-1);
	}
	m_command_str = s2e()->getConfig()->getString(
			getConfigKey() + ".command_str", "", &ok);
	if (!ok) {
		s2e()->getWarningsStream() << "You must specifiy"
				<< getConfigKey() + ".command_str" << "in XXX{seed}XXX form\n";
		exit(-1);
	}
	s2e()->getCorePlugin()->onStateKill.connect(
	sigc::mem_fun(*this, &AutoShFileGenerator::onStateKill));

	generateCmdFile();
}
void AutoShFileGenerator::generateCmdFile()
{
	std::ofstream cfile(m_command_file.c_str());
	//Here we give a random replace so that we can randomly mark symbolic in s2ecmd
	m_current_command = replace_randtemplate(m_command_str);
	cfile << m_current_command;
	cfile.close();
}
void AutoShFileGenerator::onStateKill(S2EExecutionState* state)
{
	generateCmdFile();
}
std::string AutoShFileGenerator::replace_randtemplate(std::string in_str)
{
	std::string out_str = in_str;
	int pos = in_str.find(m_randtemplate);
	std::stringstream target;
	target << rand() % 65536;
	if (in_str.find(m_randtemplate) != std::string::npos) {
		out_str = in_str.replace(pos, m_randtemplate.length(), target.str());
	}
	return out_str;
}
} /* namespace plugins */
} /* namespace s2e */
