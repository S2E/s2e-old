#ifndef S2ETOOLS_EXECTRACER_LOGPARSER_H
#define S2ETOOLS_EXECTRACER_LOGPARSER_H

#include <string>
#include <sigc++/sigc++.h>
#include <s2e/plugins/ExecutionTracers/TraceEntries.h>
#include <stdio.h>
#include <vector>

namespace s2etools
{

/**
 * Questions:
 *
 * How to represent forks ?
 * fork = progcnt + list of new ids?
 *
 */


class LogParser
{
private:

    std::vector<uint64_t> m_ItemOffsets;

public:
    LogParser();
    ~LogParser();

    sigc::signal<void,
        unsigned,
        const s2e::plugins::ExecutionTraceItemHeader &,
        void *
    >onEachItem;

    sigc::signal<
            void,
            unsigned,
            const s2e::plugins::ExecutionTraceItemHeader &,
            const s2e::plugins::ExecutionTraceModuleLoad &
    >onModuleLoadItem;

    sigc::signal<
            void,
            unsigned,
            const s2e::plugins::ExecutionTraceItemHeader &,
            const s2e::plugins::ExecutionTraceModuleUnload &
    >onModuleUnloadItem;

    sigc::signal<
            void,
            unsigned,
            const s2e::plugins::ExecutionTraceItemHeader &,
            const s2e::plugins::ExecutionTraceCall &
    >onCallItem;

    sigc::signal<
            void,
            unsigned,
            const s2e::plugins::ExecutionTraceItemHeader &,
            const s2e::plugins::ExecutionTraceReturn &
    >onReturnItem;

    bool parse(const std::string &file);
};

}

#endif
