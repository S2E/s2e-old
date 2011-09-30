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

#ifndef S2E_PLUGINS_MEMORYCHECKER_H
#define S2E_PLUGINS_MEMORYCHECKER_H

#include <s2e/Plugin.h>
#include <s2e/Plugins/CorePlugin.h>
#include <s2e/S2EExecutionState.h>

#include <string>

namespace s2e {

class ModuleDescriptor;

namespace plugins {

class OSMonitor;
class ModuleExecutionDetector;

// MemoryCheker tracks memory that the module is allowed to acces
// and checks each access for validiry
class MemoryChecker : public Plugin
{
    S2E_PLUGIN

    OSMonitor *m_osMonitor;
    ModuleExecutionDetector *m_moduleDetector;

    std::string m_moduleId;

    bool m_checkMemoryLeaks;
    bool m_checkMemoryErrors;

    bool m_terminateOnLeaks;
    bool m_terminateOnErrors;

    sigc::connection m_dataMemoryAccessConnection;

    void onModuleLoad(S2EExecutionState* state,
                      const ModuleDescriptor &module);

    void onModuleUnload(S2EExecutionState* state,
                        const ModuleDescriptor &module);

    void onModuleTransition(S2EExecutionState *state,
                            const ModuleDescriptor *prevModule,
                            const ModuleDescriptor *nextModule);

    void onDataMemoryAccess(S2EExecutionState *state,
                 klee::ref<klee::Expr> virtualAddress,
                 klee::ref<klee::Expr> hostAddress,
                 klee::ref<klee::Expr> value,
                 bool isWrite, bool isIO);

    // Simple pattern matching for region types. Only one
    // operator is allowed: '*' at the end of pattern means any
    // number of any characters.
    bool matchRegionType(const std::string &pattern, const std::string &type);

public:
    enum Permissions {
        NONE=0, READ=1, WRITE=2, READWRITE=3
    };

    MemoryChecker(S2E* s2e): Plugin(s2e) {}

    void initialize();

    void grantMemory(S2EExecutionState *state,
                     const ModuleDescriptor *module,
                     uint64_t start, uint64_t size, uint8_t perms,
                     const std::string &regionType, uint64_t regionID = 0,
                     bool permanent = false);

    // Revoke memory by address
    // NOTE: end, perms and regionID can be -1, regionTypePattern can be NULL
    bool revokeMemory(S2EExecutionState *state,
                      const ModuleDescriptor *module,
                      uint64_t start, uint64_t size,
                      uint8_t perms = uint8_t(-1),
                      const std::string &regionTypePattern = "",
                      uint64_t regionID = uint64_t(-1));

    // Revoke memory by pattern
    // NOTE: regionID can be -1
    bool revokeMemory(S2EExecutionState *state,
                      const ModuleDescriptor *module,
                      const std::string &regionTypePattern,
                      uint64_t regionID = uint64_t(-1));

    // Check acceessibility of memory region
    bool checkMemoryAccess(S2EExecutionState *state,
                           const ModuleDescriptor *module,
                           uint64_t start, uint64_t size, uint8_t perms);

    // Check that all memory objects were freed
    bool checkMemoryLeaks(S2EExecutionState *state,
                          const ModuleDescriptor *module);
};

} // namespace plugins
} // namespace s2e

#endif // S2E_PLUGINS_EXAMPLE_H
