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

namespace s2e {
namespace plugins {

S2E_DEFINE_PLUGIN(AndroidMonitor, "Plugin for tracing Android events", "AndroidMonitor");

AndroidMonitor::~AndroidMonitor()
{

}

void AndroidMonitor::initialize()
{
    std::vector<std::string> Sections;
    Sections = s2e()->getConfig()->getListKeys(getConfigKey());
    bool noErrors = true;


    if (!noErrors) {
        s2e()->getWarningsStream() << "Errors while scanning the AndroidMonitor sections"
            <<std::endl;
        exit(-1);
    }

    s2e()->getCorePlugin()->onCustomInstruction.connect(
            sigc::mem_fun(*this, &AndroidMonitor::onCustomInstruction));

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

} // namespace plugins
} // namespace s2e
