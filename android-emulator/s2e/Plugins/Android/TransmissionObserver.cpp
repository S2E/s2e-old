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
 *  This plugin allows to detect data leaks in an AndroidMonitor which incorporated the Leakalizer plugin extension (LPE).
 *	Requires AndroidAnnotation for auto-symbex.
 *
 *  RESERVES THE CUSTOM OPCODE 0xBD
 */


extern "C" {
#include "config.h"
#include "qemu-common.h"
}


#include <s2e/S2E.h>
#include <s2e/Utils.h>
#include <s2e/ConfigFile.h>
#include <s2e/Plugins/Android/TransmissionObserver.h>
#include <s2e/Plugins/Opcodes.h>
#include <sstream>

using namespace std;
using namespace s2e;
using namespace s2e::android;
using namespace s2e::linuxos;
using namespace s2e::plugins;

namespace s2e {
namespace plugins {

S2E_DEFINE_PLUGIN(TransmissionObserver, "Plugin to detect data leaks", "TransmissionObserver" /*, "AndroidAnnotation"*/);

TransmissionObserver::~TransmissionObserver()
{

}


void TransmissionObserver::initialize()
{
//
//    m_androidMonitor = static_cast<AndroidMonitor*>(s2e()->getPlugin("AndroidMonitor"));
//    assert(m_androidMonitor);

    s2e()->getCorePlugin()->onCustomInstruction.connect(
            sigc::mem_fun(*this, &TransmissionObserver::onCustomInstruction));

}


void TransmissionObserver::onCustomInstruction(S2EExecutionState* state, uint64_t opcode)
{
    if (!OPCODE_CHECK(opcode, TRANSMISSION_OBSERVER_OPCODE)) {
        return;
    }

    uint8_t op = (opcode>>8) & 0xFF; // 0xFFBD0401 --> extract 04
    uint8_t op2 = (opcode) & 0xFF;   // 0xFFBD0401 --> extract 01

	bool ok = true;

    switch(op) {
        case 0: { /* void s2e_check_leak(void *buf, unsigned size, const char* hostname, int port); */
        	uint32_t bufaddr;
        	uint32_t size;
        	std::string hostname;
        	int port;
        	GET_REGISTER_CONCRETE(0,bufaddr,4);
        	GET_REGISTER_CONCRETE(1,size,4);
        	GET_STRING_FROM_REGISTER(2,hostname,"<no destination>");
        	GET_REGISTER_CONCRETE(3,port,4);
        	if (!ok) {
        		return;
        	}
    		klee::ref<klee::Expr> expr;
    		if (isSymbolic(state,bufaddr,size, expr)) {
    			if (parseExprToFindLeak(expr)) {
    				s2e()->getMessagesStream() << "DATA LEAK to destination " << hostname << " and port" << std::dec << port << "." << endl;
    			}
    		}
        	break;
        }
    default:
    	//should not happen
    	assert(false);
    	break;
    }
}

bool TransmissionObserver::isSensitiveData(std::string &tagname) {
	return string::npos != tagname.rfind("AKAtest") || string::npos != tagname.rfind("Location") ||
		   string::npos != tagname.rfind("longitude") || string::npos != tagname.rfind("latitude") ||
		   string::npos != tagname.rfind("deviceid");
}

bool TransmissionObserver::parseExprToFindLeak(klee::ref<klee::Expr> expr) {
	bool result = false;
	klee::Expr * pExpr = expr.get();
	klee::Expr::Kind kind = pExpr->getKind();
	s2e()->getMessagesStream() << "Kind: " << kind << endl;
	switch(kind) {
		case klee::Expr::Read: {
			klee::ReadExpr *re = dyn_cast<klee::ReadExpr>(expr);
			string dataname = getNameFromReadExpression(re);
			if (isSensitiveData(dataname)) {
				s2e()->getMessagesStream() << "Data leak of type " << dataname << " detected." << endl;
				result |= true;
			}

		}
		default:
			//do nothing
		break;
	}

	for(unsigned i = 0; i < pExpr->getNumKids(); i++) {
		result |= parseExprToFindLeak(pExpr->getKid(i));
	}
	return result;
}

string TransmissionObserver::getNameFromReadExpression(klee::ReadExpr *rexpr) {
	return rexpr->updates.root->name;
}

bool TransmissionObserver::isSymbolic(S2EExecutionState* state, uint32_t addr, uint32_t size, klee::ref<klee::Expr> &val) {

	void *dummy = malloc(size);

	if (!state->readMemoryConcrete(addr, dummy, size)) {
		val = state->readMemory(addr, size*8);
		free(dummy);
		return true;
	}
	val = 0;
	free(dummy);
	return false;
}


} // namespace plugins
} // namespace s2e
