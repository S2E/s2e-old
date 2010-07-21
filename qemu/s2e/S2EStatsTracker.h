#ifndef S2E_STATSTRACKER_H
#define S2E_STATSTRACKER_H

#include <klee/Statistic.h>
#include <klee/StatsTracker.h>

namespace klee {
namespace stats {
    extern klee::Statistic translationBlocks;
    extern klee::Statistic translationBlocksConcrete;
    extern klee::Statistic translationBlocksKlee;
} // namespace stats
} // namespace klee

namespace s2e {

class S2EStatsTracker: public klee::StatsTracker
{
public:
    S2EStatsTracker(klee::Executor &_executor, std::string _objectFilename,
                    bool _updateMinDistToUncovered)
        : StatsTracker(_executor, _objectFilename, _updateMinDistToUncovered) {}

protected:
    void writeStatsHeader();
    void writeStatsLine();
};

} // namespace s2e

#endif // S2ESTATSTRACKER_H
