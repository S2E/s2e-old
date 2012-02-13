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

#ifndef S2ETOOLS_EXECTRACER_MODULEPARSER_H
#define S2ETOOLS_EXECTRACER_MODULEPARSER_H

#include <string>
#include <map>
#include <set>
#include <inttypes.h>
#include <ostream>
#include <cassert>

#include "LogParser.h"

namespace s2etools
{

struct ModuleInstance
{
    uint64_t Pid;
    uint64_t LoadBase;
    uint64_t ImageBase;
    uint64_t Size; //Used only for lookup
    std::string Name;

    ModuleInstance(
            const std::string &name, uint64_t pid, uint64_t loadBase, uint64_t size, uint64_t imageBase);

    bool operator<(const ModuleInstance& s) const {
        if (Pid == s.Pid) {
            return LoadBase + Size <= s.LoadBase;
        }
        return Pid < s.Pid;
    }

    void print(std::ostream &os) const;
};

struct ModuleInstanceCmp {
    bool operator()(const ModuleInstance *s1, const ModuleInstance *s2) const {
        if (s1->Pid == s2->Pid) {
             return s1->LoadBase + s1->Size <= s2->LoadBase;
        }
        return s1->Pid < s2->Pid;
    }
};

typedef std::set<ModuleInstance*, ModuleInstanceCmp> ModuleInstanceSet;

//Represents all the loaded modules at a given time
class ModuleCache
{
private:
    LogEvents *m_events;

    void onItem(unsigned traceIndex,
                const s2e::plugins::ExecutionTraceItemHeader &hdr,
                void *item);

public:
    ModuleCache(LogEvents *Events);
};


class ModuleCacheState: public ItemProcessorState
{
private:
    ModuleInstanceSet m_Instances;

public:
    static ItemProcessorState *factory();
    ModuleCacheState();
    virtual ~ModuleCacheState();
    virtual ItemProcessorState *clone() const;

    bool loadModule(const std::string &name, uint64_t pid, uint64_t loadBase,
                    uint64_t imageBase, uint64_t size);
    bool unloadModule(uint64_t pid, uint64_t loadBase);

    const ModuleInstance *getInstance(uint64_t pid, uint64_t pc) const;

    friend class ModuleCache;
};


struct InstructionDescriptor
{
    const ModuleInstance *m;
    uint64_t loadBase; //xxx: fixme
    uint64_t pid, pc;

    bool operator < (const InstructionDescriptor &s) const {
        if (pid == s.pid) {
            return pc < s.pc;
        }
        return pid < s.pid;
    }
};

}

#endif
