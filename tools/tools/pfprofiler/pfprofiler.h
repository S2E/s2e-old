#ifndef S2ETOOLS_PFPROFILER_H
#define S2ETOOLS_PFPROFILER_H

#include <lib/ExecutionTracer/LogParser.h>
#include <lib/ExecutionTracer/ModuleParser.h>

#include <ostream>

#include <lib/BinaryReaders/Library.h>

namespace s2etools
{

class PfProfiler
{
private:
    std::string m_FileName;
    s2etools::LogParser m_Parser;

    s2etools::ModuleLibrary m_Library;
    BFDLibrary m_binaries;

    s2etools::ModuleCache *m_ModuleCache;


    void processCallItem(unsigned traceIndex,
                         const s2e::plugins::ExecutionTraceItemHeader &hdr,
                         const s2e::plugins::ExecutionTraceCall &call);




public:
    PfProfiler(const std::string &file);
    ~PfProfiler();

    void process();
    void icountStats();
};

}

#endif
