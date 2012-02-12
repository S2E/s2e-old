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

#include "StackChecker.h"
#include <s2e/S2E.h>
#include <s2e/S2EExecutor.h>
#include <s2e/ConfigFile.h>
#include <s2e/Utils.h>

#include <sstream>

namespace s2e {
namespace plugins {

S2E_DEFINE_PLUGIN(StackChecker, "Verfies the correct stack use", "", "MemoryChecker", "StackMonitor");

void StackChecker::initialize()
{
    m_stackMonitor = static_cast<StackMonitor*>(s2e()->getPlugin("StackMonitor"));
    m_memoryChecker = static_cast<MemoryChecker*>(s2e()->getPlugin("MemoryChecker"));
    m_monitor = static_cast<OSMonitor*>(s2e()->getPlugin("Interceptor"));

    m_memoryChecker->onPostCheck.connect(
        sigc::mem_fun(*this, &StackChecker::onMemoryAccess));
}

void StackChecker::onMemoryAccess(S2EExecutionState *state, uint64_t address,
                                  unsigned size, bool isWrite, bool *result)
{
    //XXX: This is a hack until we grant param rights for each entry point.
    uint64_t stackBase = 0, stackSize = 0;
    m_monitor->getCurrentStack(state, &stackBase, &stackSize);
    if (address >= stackBase && (address < stackBase + stackSize)) {
        *result = true;
        return;
    }


    StackFrameInfo info;
    bool onTheStack = false;
    bool res = m_stackMonitor->getFrameInfo(state, address, onTheStack, info);

    *result = false;

    if (!onTheStack) {
        m_stackMonitor->dump(state);
        return;
    }

    //We are not accessing any valid frame
    if (!res) {
        std::stringstream err;

        err << "StackChecker: "
                << "BUG: memory range at " << hexval(address) << " of size " << hexval(size)
                << " is a stack location but cannot be accessed by instruction " << m_memoryChecker->getPrettyCodeLocation(state)
                << ": invalid frame!" << std::endl;

        if (m_memoryChecker->terminateOnErrors()) {
            s2e()->getExecutor()->terminateStateEarly(*state, err.str());
        }
    }

    *result = true;
}

} // namespace plugins
} // namespace s2e
