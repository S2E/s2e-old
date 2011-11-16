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

#include "S2EStatsTracker.h"

#include <s2e/S2EExecutor.h>
#include <s2e/S2EExecutionState.h>

#include <klee/CoreStats.h>
#include <klee/SolverStats.h>
#include <klee/Internal/System/Time.h>

#include <llvm/Support/Process.h>

#include <sstream>

#include <stdio.h>
#include <inttypes.h>

#include "config.h"

#ifdef CONFIG_DARWIN
#include <mach/mach.h>
#include <mach/mach_traps.h>
#endif

#ifdef CONFIG_WIN32
#include <windows.h>
#include <psapi.h>
#endif

namespace klee {
namespace stats {
    Statistic translationBlocks("TranslationBlocks", "TBs");
    Statistic translationBlocksConcrete("TranslationBlocksConcrete", "TBsConcrete");
    Statistic translationBlocksKlee("TranslationBlocksKlee", "TBsKlee");

    Statistic cpuInstructions("CpuInstructions", "CpuI");
    Statistic cpuInstructionsConcrete("CpuInstructionsConcrete", "CpuIConcrete");
    Statistic cpuInstructionsKlee("CpuInstructionsKlee", "CpuIKlee");

    Statistic concreteModeTime("ConcreteModeTime", "ConcModeTime");
    Statistic symbolicModeTime("SymbolicModeTime", "SymbModeTime");
} // namespace stats
} // namespace klee

using namespace klee;
using namespace llvm;


namespace s2e {

/**
 *  Replaces the broken LLVM functions
 */
uint64_t S2EStatsTracker::getProcessMemoryUsage()
{
#if defined(CONFIG_WIN32)

    PROCESS_MEMORY_COUNTERS Memory;
    HANDLE CurrentProcess = GetCurrentProcess();

    if (!GetProcessMemoryInfo(CurrentProcess, &Memory, sizeof(Memory))) {
        return 0;
    }

    return Memory.PagefileUsage;

#elif defined(CONFIG_DARWIN)
    struct task_basic_info t_info;
    mach_msg_type_number_t t_info_count = TASK_BASIC_INFO_COUNT;

    if (KERN_SUCCESS != task_info(mach_task_self(),
                                  TASK_BASIC_INFO, (task_info_t)&t_info,
                                  &t_info_count))
    {
        return -1;
    }
    // resident size is in t_info.resident_size;
    //return t_info.virtual_size;
    return t_info.resident_size;

#else
    pid_t myPid = getpid();
    std::stringstream ss;
    ss << "/proc/" << myPid << "/status";

    FILE *fp = fopen(ss.str().c_str(), "r");
    if (!fp) {
        return 0;
    }

    uint64_t peakMem=0;

    char buffer[512];
    while(!peakMem && fgets(buffer, sizeof(buffer), fp)) {
        if (sscanf(buffer, "VmSize: %" PRIu64, &peakMem)) {
            break;
        }
    }

    fclose(fp);

    return peakMem * 1024;
#endif
}

void S2EStatsTracker::writeStatsHeader() {
  *statsFile //<< "('Instructions',"
             //<< "'FullBranches',"
             //<< "'PartialBranches',"
             //<< "'NumBranches',"
             << "('NumStates',"
             << "'NumQueries',"
             << "'NumQueryConstructs',"
             << "'NumObjects',"
             //<< "'CoveredInstructions',"
             //<< "'UncoveredInstructions',"
             << "'TranslationBlocks',"
             << "'TranslationBlocksConcrete',"
             << "'TranslationBlocksKlee',"
             << "'CpuInstructions',"
             << "'CpuInstructionsConcrete',"
             << "'CpuInstructionsKlee',"
             << "'ConcreteModeTime',"
             << "'SymbolicModeTime',"
             << "'UserTime',"
             << "'WallTime',"
             << "'QueryTime',"
             << "'SolverTime',"
             << "'CexCacheTime',"
             << "'ForkTime',"
             << "'ResolveTime',"
             << "'MemoryUsage',"
             << ")\n";
  statsFile->flush();
}

void S2EStatsTracker::writeStatsLine() {
  *statsFile //<< "(" << stats::instructions
             //<< "," << fullBranches
             //<< "," << partialBranches
             //<< "," << numBranches
             << "(" << executor.getStatesCount()
             << "," << stats::queries
             << "," << stats::queryConstructs
             << "," << 0 // was numObjects
             //<< "," << stats::coveredInstructions
             //<< "," << stats::uncoveredInstructions
             << "," << stats::translationBlocks
             << "," << stats::translationBlocksConcrete
             << "," << stats::translationBlocksKlee
             << "," << stats::cpuInstructions
             << "," << stats::cpuInstructionsConcrete
             << "," << stats::cpuInstructionsKlee
             << "," << stats::concreteModeTime / 1000000.
             << "," << stats::symbolicModeTime / 1000000.
             << "," << util::getUserTime()
             << "," << elapsed()
             << "," << stats::queryTime / 1000000.
             << "," << stats::solverTime / 1000000.
             << "," << stats::cexCacheTime / 1000000.
             << "," << stats::forkTime / 1000000.
             << "," << stats::resolveTime / 1000000.
             << "," << getProcessMemoryUsage() //sys::Process::GetTotalMemoryUsage()
             << ")\n";
  statsFile->flush();
}

S2EStateStats::S2EStateStats():
    m_statTranslationBlockConcrete(0),
    m_statTranslationBlockSymbolic(0),
    m_statInstructionCountSymbolic(0),
    m_laststatTranslationBlockConcrete(0),
    m_laststatTranslationBlockSymbolic(0),
    m_laststatInstructionCount(0),
    m_laststatInstructionCountConcrete(0),
    m_laststatInstructionCountSymbolic(0)
{

}

void S2EStateStats::updateStats(S2EExecutionState* state)
{
    //Updating translation block counts
    uint64_t tbcdiff = m_statTranslationBlockConcrete - m_laststatTranslationBlockConcrete;
    stats::translationBlocksConcrete += tbcdiff;
    m_laststatTranslationBlockConcrete = m_statTranslationBlockConcrete;

    uint64_t sbcdiff = m_statTranslationBlockSymbolic - m_laststatTranslationBlockSymbolic;
    stats::translationBlocksKlee += sbcdiff;
    m_laststatTranslationBlockSymbolic = m_statTranslationBlockSymbolic;

    stats::translationBlocks += tbcdiff + sbcdiff;

    //Updating instruction counts

    //KLEE icount
    uint64_t sidiff = m_statInstructionCountSymbolic - m_laststatInstructionCountSymbolic;
    stats::cpuInstructionsKlee += sidiff;
    m_laststatInstructionCountSymbolic = m_statInstructionCountSymbolic;

    //Total icount
    uint64_t totalICount = state->getTotalInstructionCount();
    uint64_t tidiff = totalICount - m_laststatInstructionCount;
    stats::cpuInstructions += tidiff;
    m_laststatInstructionCount = totalICount;

    //Concrete icount
    uint64_t ccount = totalICount - m_statInstructionCountSymbolic;
    uint64_t cidiff = ccount - m_laststatInstructionCountConcrete;
    stats::cpuInstructionsConcrete += cidiff;
    m_laststatInstructionCountConcrete = ccount;
}


} // namespace s2e
