/*
 * AutoShFileGenerator.cpp
 *
 *  Created on: 2015年12月23日
 *      Author: Epeius
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
S2E_DEFINE_PLUGIN(AutoShFileGenerator, "AutoShFileGenerator plugin", "AutoShFileGenerator",
		);

AutoShFileGenerator::AutoShFileGenerator(S2E* s2e) :
		Plugin(s2e) {
	m_current_command = " ";
	m_randtemplate = "{seed}";
}

AutoShFileGenerator::~AutoShFileGenerator() {
}
void AutoShFileGenerator::initialize() {
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
void AutoShFileGenerator::generateCmdFile() {
	std::ofstream cfile(m_command_file.c_str());
	//替换其中需要随机化的变量
	m_current_command = replace_randtemplate(m_command_str);
	cfile << m_current_command;
	cfile.close();
}
void AutoShFileGenerator::onStateKill(S2EExecutionState* state) {
	generateCmdFile();
}
std::string AutoShFileGenerator::replace_randtemplate(std::string in_str)
{
	std:: string out_str = in_str;
    int pos = in_str.find(m_randtemplate);
    std::stringstream target;
    target << rand() % 65536;
    if (in_str.find(m_randtemplate) != std::string::npos){
        out_str = in_str.replace(pos, m_randtemplate.length(), target.str());
    }
    return out_str;
}
} /* namespace plugins */
} /* namespace s2e */
