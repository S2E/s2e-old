#include "S2EStatsTracker.h"

#include <s2e/S2EExecutor.h>

#include <klee/CoreStats.h>
#include "klee/SolverStats.h"
#include <klee/Internal/System/Time.h>

#include <llvm/System/Process.h>

#include <sstream>

#include <stdio.h>
#include <inttypes.h>

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
#ifdef _WIN32
#error Implement memory usage detection for Windows
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

} // namespace s2e
