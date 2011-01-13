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

#ifndef S2ETOOLS_FORKPROFILER_H
#define S2ETOOLS_FORKPROFILER_H

#include <lib/ExecutionTracer/LogParser.h>
#include <lib/ExecutionTracer/ModuleParser.h>

#include <lib/BinaryReaders/Library.h>

#include <ostream>

namespace s2etools
{

class ForkProfiler
{
public:

    struct Fork {
        uint32_t id;
        uint64_t pid;
        uint64_t relPc, pc;
        std::string module;
        std::vector<uint32_t> children;
    };

    struct ForkPoint {
        uint64_t pc, pid;
        uint64_t count;
        uint64_t line;
        std::string file, function, module;
        uint64_t loadbase, imagebase;

        bool operator()(const ForkPoint &fp1, const ForkPoint &fp2) const {
            if (fp1.pid == fp2.pid) {
                return fp1.pc < fp2.pc;
            }else {
                return fp1.pid < fp2.pid;
            }
        }
    };

    struct ForkPointByCount {
        bool operator()(const ForkPoint &fp1, const ForkPoint &fp2) const {
            if (fp1.count == fp2.count) {
                if (fp1.pid == fp2.pid) {
                    return fp1.pc < fp2.pc;
                }else {
                    return fp1.pid < fp2.pid;
                }
            }else {
                return fp1.count < fp2.count;
            }
        }
    };

    typedef std::vector<Fork> ForkList;
    typedef std::set<ForkPoint, ForkPoint> ForkPoints;
    typedef std::set<ForkPoint, ForkPointByCount> ForkPointsByCount;
private:
    LogEvents *m_events;
    ModuleCache *m_cache;
    Library *m_library;

    sigc::connection m_connection;
    ForkList m_forks;
    ForkPoints m_forkPoints;

    void onItem(unsigned traceIndex,
                const s2e::plugins::ExecutionTraceItemHeader &hdr,
                void *item);

    void doProfile(
            const s2e::plugins::ExecutionTraceItemHeader &hdr,
            const s2e::plugins::ExecutionTraceFork *te);
    void doGraph(
            const s2e::plugins::ExecutionTraceItemHeader &hdr,
            const s2e::plugins::ExecutionTraceFork *te);

public:
    ForkProfiler(Library *lib, ModuleCache *cache, LogEvents *events);
    virtual ~ForkProfiler();

    void process();

    void outputProfile(const std::string &path) const;
    void outputGraph(const std::string &path) const;
};

}

#endif
