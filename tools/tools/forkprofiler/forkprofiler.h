#ifndef S2ETOOLS_FORKPROFILER_H
#define S2ETOOLS_FORKPROFILER_H

#include <lib/ExecutionTracer/LogParser.h>
#include <lib/ExecutionTracer/ModuleParser.h>

#include <lib/BinaryReaders/Library.h>

#include <ostream>

namespace s2etools
{

class ForkProfiler
{
public:

    struct ForkPoint {
        uint64_t pc, pid;
        uint64_t count;
        uint64_t line;
        std::string file, function, module;
        uint64_t loadbase, imagebase;

        bool operator()(const ForkPoint &fp1, const ForkPoint &fp2) const {
            if (fp1.pid == fp2.pid) {
                return fp1.pc < fp2.pc;
            }else {
                return fp1.pid < fp2.pid;
            }
        }
    };

    struct ForkPointByCount {
        bool operator()(const ForkPoint &fp1, const ForkPoint &fp2) const {
            if (fp1.count == fp2.count) {
                if (fp1.pid == fp2.pid) {
                    return fp1.pc < fp2.pc;
                }else {
                    return fp1.pid < fp2.pid;
                }
            }else {
                return fp1.count < fp2.count;
            }
        }
    };

    typedef std::set<ForkPoint, ForkPoint> ForkPoints;
    typedef std::set<ForkPoint, ForkPointByCount> ForkPointsByCount;
private:
    LogEvents *m_events;
    ModuleCache *m_cache;
    Library *m_library;

    sigc::connection m_connection;

    ForkPoints m_forkPoints;

    void onItem(unsigned traceIndex,
                const s2e::plugins::ExecutionTraceItemHeader &hdr,
                void *item);

public:
    ForkProfiler(Library *lib, ModuleCache *cache, LogEvents *events);
    virtual ~ForkProfiler();

    void process();

    void outputProfile(const std::string &Path) const;

};

}

#endif
