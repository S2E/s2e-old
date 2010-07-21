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
} // namespace stats
} // namespace klee

using namespace klee;
using namespace llvm;

namespace s2e {

void S2EStatsTracker::writeStatsHeader() {
  *statsFile << "('Instructions',"
             << "'TranslationBlocks',"
             << "'TranslationBlocksConcrete',"
             << "'TranslationBlocksKlee',"
             << "'FullBranches',"
             << "'PartialBranches',"
             << "'NumBranches',"
             << "'UserTime',"
             << "'NumStates',"
             << "'MallocUsage',"
             << "'NumQueries',"
             << "'NumQueryConstructs',"
             << "'NumObjects',"
             << "'WallTime',"
             << "'CoveredInstructions',"
             << "'UncoveredInstructions',"
             << "'QueryTime',"
             << "'SolverTime',"
             << "'CexCacheTime',"
             << "'ForkTime',"
             << "'ResolveTime',"
             << ")\n";
  statsFile->flush();
}

void S2EStatsTracker::writeStatsLine() {
  *statsFile << "(" << stats::instructions
             << "," << stats::translationBlocks
             << "," << stats::translationBlocksConcrete
             << "," << stats::translationBlocksKlee
             << "," << fullBranches
             << "," << partialBranches
             << "," << numBranches
             << "," << util::getUserTime()
             << "," << executor.getStatesCount()
             << "," << sys::Process::GetTotalMemoryUsage()
             << "," << stats::queries
             << "," << stats::queryConstructs
             << "," << 0 // was numObjects
             << "," << elapsed()
             << "," << stats::coveredInstructions
             << "," << stats::uncoveredInstructions
             << "," << stats::queryTime / 1000000.
             << "," << stats::solverTime / 1000000.
             << "," << stats::cexCacheTime / 1000000.
             << "," << stats::forkTime / 1000000.
             << "," << stats::resolveTime / 1000000.
             << ")\n";
  statsFile->flush();
}

} // namespace s2e
