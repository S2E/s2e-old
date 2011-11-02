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

#ifndef _ANDROIDANNOTATION_PLUGIN_H_

#define _ANDROIDANNOTATION_PLUGIN_H_

#include <s2e/Plugin.h>
#include <s2e/Plugins/CorePlugin.h>
#include <s2e/S2EExecutionState.h>
#include <s2e/Plugins/Android/AndroidMonitor.h>
#include <vector>

namespace s2e {
namespace plugins {

class AndroidAnnotation:public Plugin
{
    S2E_PLUGIN

private:

	typedef std::string MethodDescriptor;			//something like: Lch/epfl/s2e/android/S2EAndroidActivity;.sendToLocation
	                   	   	   	   	   	   	   	    //                L<packagename>/<classname>;.<methodname>
	typedef std::set<MethodDescriptor> MethodSet;

	AndroidMonitor * m_androidMonitor;
	MethodSet m_methodsToSymbex;			//a list of methods which should be symbexed


    void onCustomInstruction(S2EExecutionState* state, uint64_t opcode);
    void initUnit(std::string unit_name, std::vector<std::string> Sections);
    bool addMethodToSymbex(MethodDescriptor m);
    bool shouldMethodBeSymbexed(MethodDescriptor m);

public:

    AndroidAnnotation(S2E* s2e): Plugin(s2e) {};
    virtual ~AndroidAnnotation();
    void initialize();


};

} // namespace plugins
} // namespace s2e

#endif
