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
 *    Volodymyr Kuznetsov <vova.kuznetsov@epfl.ch>
 *    Vitaly Chipounov <vitaly.chipounov@epfl.ch>
 *
 * All contributors are listed in S2E-AUTHORS file.
 *
 */

#ifndef S2E_STATSTRACKER_H
#define S2E_STATSTRACKER_H

#include <klee/Statistic.h>
#include <klee/StatsTracker.h>

namespace klee {
namespace stats {
    extern klee::Statistic translationBlocks;
    extern klee::Statistic translationBlocksConcrete;
    extern klee::Statistic translationBlocksKlee;

    extern klee::Statistic cpuInstructions;
    extern klee::Statistic cpuInstructionsConcrete;
    extern klee::Statistic cpuInstructionsKlee;

    extern klee::Statistic concreteModeTime;
    extern klee::Statistic symbolicModeTime;
} // namespace stats
} // namespace klee

namespace s2e {

class S2EStatsTracker: public klee::StatsTracker
{
public:
    S2EStatsTracker(klee::Executor &_executor, std::string _objectFilename,
                    bool _updateMinDistToUncovered)
        : StatsTracker(_executor, _objectFilename, _updateMinDistToUncovered) {}

    static uint64_t getProcessMemoryUsage();
protected:
    void writeStatsHeader();
    void writeStatsLine();
};

class S2EExecutionState;

class S2EStateStats {
public:

    //Statistics counters
    uint64_t m_statTranslationBlockConcrete;
    uint64_t m_statTranslationBlockSymbolic;
    uint64_t m_statInstructionCountSymbolic;

    //Counter values at the last check
    uint64_t m_laststatTranslationBlockConcrete;
    uint64_t m_laststatTranslationBlockSymbolic;
    uint64_t m_laststatInstructionCount;
    uint64_t m_laststatInstructionCountConcrete;
    uint64_t m_laststatInstructionCountSymbolic;

public:
    S2EStateStats();
    void updateStats(S2EExecutionState* state);

};

} // namespace s2e

#endif // S2ESTATSTRACKER_H
