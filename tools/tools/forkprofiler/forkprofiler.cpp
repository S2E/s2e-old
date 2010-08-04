#define __STDC_FORMAT_MACROS 1

#include "llvm/Support/CommandLine.h"

#include <lib/ExecutionTracer/ModuleParser.h>
#include <lib/ExecutionTracer/Path.h>
#include <lib/ExecutionTracer/TestCase.h>
#include <lib/BinaryReaders/BFDInterface.h>

#include <s2e/Plugins/ExecutionTracers/TraceEntries.h>

#include <stdio.h>
#include <ostream>
#include <fstream>
#include <iostream>
#include <sstream>
#include <inttypes.h>
#include <iomanip>
#include "forkprofiler.h"

using namespace llvm;
using namespace s2etools;


namespace {


cl::opt<std::string>
    TraceFile("trace", cl::desc("Input trace"), cl::init("ExecutionTracer.dat"));

cl::opt<std::string>
    LogDir("outputdir", cl::desc("Store the coverage into the given folder"), cl::init("."));

cl::opt<std::string>
    ModPath("modpath", cl::desc("Path to module descriptors"), cl::init("."));

}

namespace s2etools
{
ForkProfiler::ForkProfiler(Library *lib, ModuleCache *cache, LogEvents *events)
{
    m_events = events;
    m_connection = events->onEachItem.connect(
            sigc::mem_fun(*this, &ForkProfiler::onItem)
            );
    m_cache = cache;
    m_library = lib;
}

ForkProfiler::~ForkProfiler()
{
    m_connection.disconnect();
}

void ForkProfiler::onItem(unsigned traceIndex,
            const s2e::plugins::ExecutionTraceItemHeader &hdr,
            void *item)
{
    if (hdr.type != s2e::plugins::TRACE_FORK) {
        return;
    }

    const s2e::plugins::ExecutionTraceFork *te =
            (const s2e::plugins::ExecutionTraceFork*) item;

    ModuleCacheState *mcs = static_cast<ModuleCacheState*>(m_events->getState(m_cache, &ModuleCacheState::factory));

    const ModuleInstance *mi = mcs->getInstance(hdr.pid, te->pc);

    ForkPoint fp;
    fp.pc = te->pc;
    fp.pid = hdr.pid;
    fp.count = 1;
    fp.line = 0;

    ForkPoints::iterator it = m_forkPoints.find(fp);
    if (it == m_forkPoints.end()) {
        if (mi) {
            m_library->getInfo(mi, te->pc, fp.file, fp.line, fp.function);
            fp.module = mi->Name;
            fp.loadbase = mi->LoadBase;
            fp.imagebase = mi->ImageBase;
        }
        m_forkPoints.insert(fp);
    }else {
        fp = *it;
        m_forkPoints.erase(*it);
        fp.count++;
        m_forkPoints.insert(fp);
    }

}

void ForkProfiler::outputProfile(const std::string &path) const
{
    std::stringstream ss;
    ss << path << "/" << "forkprofile.txt";
    std::ofstream forkProfile(ss.str().c_str());

    ForkPointsByCount fpCnt;

    ForkPoints::const_iterator it;
    for (it = m_forkPoints.begin(); it != m_forkPoints.end(); ++it) {
        fpCnt.insert(*it);
    }

    forkProfile << "#Pc      \tModule\tForkCnt\tSource\tFunction\tLine" << std::endl;

    ForkPointsByCount::const_iterator cit;
    for (cit = fpCnt.begin(); cit != fpCnt.end(); ++cit) {
        const ForkPoint &fp = *cit;
        forkProfile << std::hex << "0x" << std::setw(8) << std::setfill('0') << (fp.pc - fp.loadbase + fp.imagebase) << "\t";
        forkProfile << std::setfill(' ');
        if (fp.module.size() > 0) {
            forkProfile << fp.module << "\t";
        }else {
            forkProfile << "?\t";
        }

        forkProfile << std::dec << fp.count << "\t";

        if (fp.file.size() > 0) {
            forkProfile << fp.file << "\t";
        }else {
            forkProfile << "?\t";
        }

        if (fp.function.size() > 0) {
            forkProfile << fp.function << "\t";
        }else {
            forkProfile << "?\t";
        }

        forkProfile << std::dec << fp.line << std::endl;
    }
}


}


int main(int argc, char **argv)
{
    cl::ParseCommandLineOptions(argc, (char**) argv, " debugger");

    Library library;
    library.setPath(ModPath);

    LogParser parser;
    PathBuilder pb(&parser);
    parser.parse(TraceFile);

    ModuleCache mc(&pb);
    ForkProfiler fp(&library, &mc, &pb);

    pb.processTree();

    fp.outputProfile(LogDir);

    return 0;
}
