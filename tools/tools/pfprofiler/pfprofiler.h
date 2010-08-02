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

    Library m_binaries;

    s2etools::ModuleCache *m_ModuleCache;




public:
    PfProfiler(const std::string &file);
    ~PfProfiler();

    void process();
    void extractAggregatedData();
};

}

#endif
