#include "S2EStatsTracker.h"

#include <s2e/S2EExecutor.h>

#include <klee/CoreStats.h>
#include "klee/SolverStats.h"
#include <klee/Internal/System/Time.h>

#include <llvm/System/Process.h>

namespace klee {
namespace stats {
    Statistic translationBlocks("TranslationBlocks", "TBs");
    Statistic translationBlocksConcrete("TranslationBlocksConcrete", "TBsConcrete");
    Statistic translationBlocksKlee("TranslationBlocksKlee", "TBsKlee");

    Statistic concreteModeTime("ConcreteModeTime", "ConcModeTime");
    Statistic symbolicModeTime("SymbolicModeTime", "SymbModeTime");
} // namespace stats
} // namespace klee

using namespace klee;
using namespace llvm;

namespace s2e {

void S2EStatsTracker::writeStatsHeader() {
  *statsFile << "('Instructions',"
             << "'FullBranches',"
             << "'PartialBranches',"
             << "'NumBranches',"
             << "'NumStates',"
             << "'NumQueries',"
             << "'NumQueryConstructs',"
             << "'NumObjects',"
             << "'CoveredInstructions',"
             << "'UncoveredInstructions',"
             << "'TranslationBlocks',"
             << "'TranslationBlocksConcrete',"
             << "'TranslationBlocksKlee',"
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
  *statsFile << "(" << stats::instructions
             << "," << fullBranches
             << "," << partialBranches
             << "," << numBranches
             << "," << executor.getStatesCount()
             << "," << stats::queries
             << "," << stats::queryConstructs
             << "," << 0 // was numObjects
             << "," << stats::coveredInstructions
             << "," << stats::uncoveredInstructions
             << "," << stats::translationBlocks
             << "," << stats::translationBlocksConcrete
             << "," << stats::translationBlocksKlee
             << "," << stats::concreteModeTime / 1000000.
             << "," << stats::symbolicModeTime / 1000000.
             << "," << util::getUserTime()
             << "," << elapsed()
             << "," << stats::queryTime / 1000000.
             << "," << stats::solverTime / 1000000.
             << "," << stats::cexCacheTime / 1000000.
             << "," << stats::forkTime / 1000000.
             << "," << stats::resolveTime / 1000000.
             << "," << sys::Process::GetTotalMemoryUsage()
             << ")\n";
  statsFile->flush();
}

} // namespace s2e
