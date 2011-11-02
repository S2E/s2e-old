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
#include <s2e/Plugins/Android/AndroidAnnotation.h>
#include <s2e/Plugins/Opcodes.h>
#include <sstream>

using namespace std;
using namespace s2e;
using namespace s2e::android;
using namespace s2e::linuxos;
using namespace s2e::plugins;

namespace s2e {
namespace plugins {

S2E_DEFINE_PLUGIN(AndroidAnnotation, "Plugin for auto-symbexing selected Android methods", "AndroidAnnotation", "AndroidMonitor");

AndroidAnnotation::~AndroidAnnotation()
{

}

void AndroidAnnotation::initUnit(std::string unit_name, std::vector<std::string> Sections) {

	MethodDescriptor method_descriptor;
	bool ok;

    foreach2(it, Sections.begin(), Sections.end()) {
        if (*it == "symbexAll") {
        	//XXX: not yet implemented
        	continue;
        }

        s2e()->getMessagesStream() << "Scanning section " << getConfigKey() << "." << *it << std::endl;
        std::stringstream sk;
        sk << getConfigKey() << "." << unit_name << "." << *it;

        method_descriptor = (MethodDescriptor) s2e()->getConfig()->getString(sk.str() , "", &ok);

        if(!ok) {
        	s2e()->getMessagesStream() << "Could not read " << sk.str() << std::endl;
        	return;
        }
        addMethodToSymbex(method_descriptor);
    }
    s2e()->getMessagesStream() << "Unit " << unit_name << " initialized." << std::endl;
}
bool AndroidAnnotation::addMethodToSymbex(MethodDescriptor m) {
	pair<set<MethodDescriptor>::iterator,bool> ok;
	ok = m_methodsToSymbex.insert(m);
	if (!ok.second) {
		s2e()->getMessagesStream() << "already enabled symbex for method " << m << std::endl;
	} else {
		s2e()->getMessagesStream() << "Enable symbex for method " << m << std::endl;
	}
	return ok.second;
}

bool AndroidAnnotation::shouldMethodBeSymbexed(MethodDescriptor m) {

	set<MethodDescriptor>::iterator it;
	it = m_methodsToSymbex.find(m);
	if (it == m_methodsToSymbex.end()) {
		return false;
	} else {
		return true;
	}

}

void AndroidAnnotation::initialize()
{
    std::vector<std::string> Sections;
    std::string unit = "unit";
    bool ok = false;
    Sections = s2e()->getConfig()->getListKeys(getConfigKey() + "." + unit );
    initUnit(unit,Sections);

    m_androidMonitor = static_cast<AndroidMonitor*>(s2e()->getPlugin("AndroidMonitor"));
    assert(m_androidMonitor);

    s2e()->getCorePlugin()->onCustomInstruction.connect(
            sigc::mem_fun(*this, &AndroidAnnotation::onCustomInstruction));

}


void AndroidAnnotation::onCustomInstruction(S2EExecutionState* state, uint64_t opcode)
{
    if (!OPCODE_CHECK(opcode, ANDROID_ANNOTATION_OPCODE)) {
        return;
    }

    uint8_t op = (opcode>>8) & 0xFF; // 0xFFBC0401 --> extract 04
    uint8_t op2 = (opcode) & 0xFF;   // 0xFFBC0401 --> extract 01

	bool ok = true;

    switch(op) {
        case 0: { /* enable symbex for a particular method */
        	MethodDescriptor method_descriptor;
        	GET_STRING_FROM_REGISTER(0,method_descriptor,"<no method_descriptor>");
        	if (ok) {
        		addMethodToSymbex(method_descriptor);
        	}
        	break;
        }
        case 1: { /* 0xFFBC0100 check if this method should be symbexed (custom Dalvik VM then injects symb. values into the parameters ) */
			std::string clazzDescriptor; //class descriptor of the method being called next
			std::string methodName;		 //name of the method being called next
			int result = 0;
			GET_STRING_FROM_REGISTER(0,clazzDescriptor,"<no clazzdescriptor>");
			GET_STRING_FROM_REGISTER(1,methodName,"<no methodName>");

			MethodDescriptor method_descriptor = clazzDescriptor + "." + methodName;

			if(shouldMethodBeSymbexed(method_descriptor)) {
						result = 0xCAFE; //magic value understood by target-side of the plugin
						s2e()->getMessagesStream() << "AndroidAnnotation:: symbexing method "
								<< clazzDescriptor << "." << methodName << "."<< endl;
			 }

			//inform target about our decision
			state->writeCpuRegisterConcrete(CPU_OFFSET(regs[0]),&result,4);
			break;
        }
    default:
    	//should not happen
    	assert(false);
    	break;
    }
}

} // namespace plugins
} // namespace s2e
