#ifndef S2ETOOLS_TBTRACE_H
#define S2ETOOLS_TBTRACE_H

#include <lib/ExecutionTracer/LogParser.h>
#include <lib/ExecutionTracer/ModuleParser.h>

#include <ostream>
#include <fstream>

#include <lib/BinaryReaders/Library.h>

namespace s2etools
{

class TbTrace
{
public:

private:
    LogEvents *m_events;
    ModuleCache *m_cache;
    Library *m_library;
    std::ofstream &m_output;

    sigc::connection m_connection;

    bool m_hasItems;
    bool m_hasModuleInfo;
    bool m_hasDebugInfo;

    void onItem(unsigned traceIndex,
                const s2e::plugins::ExecutionTraceItemHeader &hdr,
                void *item);

    void printDebugInfo(uint64_t pid, uint64_t pc);
public:
    TbTrace(Library *lib, ModuleCache *cache, LogEvents *events, std::ofstream &ofs);
    virtual ~TbTrace();

    void outputTraces(const std::string &Path) const;
    bool hasItems() const {
        return m_hasItems;
    }

    bool hasModuleInfo() const {
        return m_hasModuleInfo;
    }

    bool hasDebugInfo() const {
        return m_hasDebugInfo;
    }

};

class TbTraceTool
{
private:
    LogParser m_parser;

    Library m_binaries;

public:
    TbTraceTool();
    ~TbTraceTool();

    void process();
    void flatTrace();
};


}

#endif
