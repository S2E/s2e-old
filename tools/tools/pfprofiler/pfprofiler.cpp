/*#define __STDC_CONSTANT_MACROS 1
#define __STDC_LIMIT_MACROS 1
#define __STDC_FORMAT_MACROS 1
*/

#include "llvm/Support/CommandLine.h"
#include <stdio.h>
#include <inttypes.h>
#include <iostream>
#include <iomanip>
#include <fstream>

#include <lib/ExecutionTracer/ModuleParser.h>
#include <lib/ExecutionTracer/Path.h>
#include <lib/ExecutionTracer/TestCase.h>
#include <lib/ExecutionTracer/PageFault.h>
#include <lib/ExecutionTracer/InstructionCounter.h>
#include <lib/BinaryReaders/BFDInterface.h>

#include "pfprofiler.h"
#include "CacheProfiler.h"

#include <malloc.h>

extern "C" {
#include <bfd.h>
}

using namespace llvm;
using namespace s2etools;
using namespace s2e::plugins;

namespace {

cl::opt<std::string>
    TraceFile("trace", cl::desc("<input trace>"), cl::init("ExecutionTracer.dat"));

cl::opt<std::string>
        ModPath("modpath", cl::desc("Path to module descriptors"), cl::init("."));

cl::opt<std::string>
        ModListRaw("modlist", cl::desc("Display debug info for the specified modules (all when empty)"), cl::init(""));
std::vector<std::string> ModList;

cl::opt<std::string>
        AnaType("type", cl::desc("Type of analysis (cache/icount/pf)"), cl::init("cache"));

cl::opt<bool>
        TerminatedPaths("termpath", cl::desc("Show paths that have a test case"), cl::init(true));


cl::opt<std::string>
        OutFile("outfile", cl::desc("Output file"), cl::init("stats.dat"));

cl::opt<std::string>
        OutDir("outdir", cl::desc("Output directory"), cl::init("."));


cl::opt<unsigned>
        CPMinMissCountFilter("cpminmisscount", cl::desc("CacheProfiler: minimum miss count to report"),
                                cl::init(0));

cl::opt<std::string>
        CPFilterProcess("cpfilterprocess", cl::desc("CacheProfiler: Report modules from this address space"),
                        cl::init(""));

cl::opt<std::string>
        CPFilterModule("cpfiltermodule", cl::desc("CacheProfiler: Report only this module"),
                        cl::init(""));

cl::opt<std::string>
        CpOutFile("cpoutfile", cl::desc("CacheProfiler: output file"),
                        cl::init("stats.dat"));

cl::opt<int>
        HtmlOutput("html", cl::desc("HTML output"),
                        cl::init(0));

}


namespace s2etools
{

PfProfiler::PfProfiler(const std::string &file)
{
    m_FileName = file;
    m_ModuleCache = NULL;
    m_Library.setPath(ModPath);
    m_binaries.setPath(ModPath);
}

PfProfiler::~PfProfiler()
{

}




void PfProfiler::processCallItem(unsigned traceIndex,
                     const s2e::plugins::ExecutionTraceItemHeader &hdr,
                     const s2e::plugins::ExecutionTraceCall &call)
{
    const ModuleInstance *msource = m_ModuleCache->getInstance(hdr.pid, call.source);
    const ModuleInstance *mdest = m_ModuleCache->getInstance(hdr.pid, call.target);

    std::cout << "Processing entry " << std::dec << traceIndex << " - ";
    std::string fcnName;
    if (msource) {
        msource->getSymbol(fcnName, call.source);
        std::cout << msource->Mod->getModuleName() << " [" << fcnName << "] " ;
    }else {
        std::cout << "<unknown> ";
    }
    std::cout << std::hex << call.source << " -> " << call.target;
    if (mdest) {
        mdest->getSymbol(fcnName, call.target);
        std::cout << " " << mdest->Mod->getModuleName() << " [" << fcnName << "] " ;
    }else {
        std::cout << " <unknown>";
    }
    std::cout << std::endl;



}

void PfProfiler::pageFaults()
{
    std::ofstream statsFile;
    std::string sFile = OutDir + "/pagefaults.stats";
    statsFile.open(sFile.c_str());

    if (TerminatedPaths) {
        statsFile << "#This report shows data for paths that generated a test case" << std::endl;
    }

    if (CPFilterModule.size() > 0) {
        statsFile << "#Showing data for module " << CPFilterModule << std::endl;
    }

    statsFile << "#PageFaults TlbMisses" << std::endl;

    ExecutionPaths paths;
    PathBuilder pb(&m_Parser);
    m_Parser.parse(m_FileName);

    pb.enumeratePaths(paths);


    ExecutionPaths::iterator pit;

    for(pit = paths.begin(); pit != paths.end(); ++pit) {
        TestCase tc(&pb);
        ModuleCache mc(&pb, &m_Library);
        PageFault pf(&pb, &mc);

         if (CPFilterModule.size() > 0) {
             pf.setModule(CPFilterModule);
         }

        pb.processPath(*pit);
        if (TerminatedPaths && !tc.hasInputs()) {
            continue;
        }

        statsFile << std::dec << pf.getPageFaults() << "\t";
        statsFile << pf.getTlbMisses();
        statsFile << std::endl;
    }

}

void PfProfiler::icountStats()
{
    std::ofstream statsFile;
    std::string sFile = OutDir + "/icount.stats";
    statsFile.open(sFile.c_str());

    statsFile << "#Each line represents execution path" << std::endl <<
            "#The count represents takes into account all the modules analyzed during symbex" << std::endl;

    if (TerminatedPaths) {
        statsFile << "#This report shows data for paths that generated a test case" << std::endl;
    }

    statsFile << "#NumInstructions" << std::endl;


    ExecutionPaths paths;
    PathBuilder pb(&m_Parser);
    m_Parser.parse(m_FileName);

    pb.enumeratePaths(paths);


    ExecutionPaths::iterator pit;
    for(pit = paths.begin(); pit != paths.end(); ++pit) {
        TestCase tc(&pb);
        InstructionCounter ic(&pb);

        pb.processPath(*pit);
        if (TerminatedPaths && !tc.hasInputs()) {
            continue;
        }

        statsFile << std::dec << ic.getCount() << "\t";
        tc.printInputsLine(statsFile);
        statsFile << std::endl;
    }
}

void PfProfiler::process()
{
    uint64_t maxMissCount=0, maxMissPath=0;
    uint64_t maxICount=0, maxICountPath=0;

    uint64_t minMissCount=(uint64_t)-1, minMissPath=0;
    uint64_t minICount=(uint64_t)-1, minICountPath=0;

    std::ofstream statsFile;
    statsFile.open(CpOutFile.c_str());

    ExecutionPaths paths;
    PathBuilder pb(&m_Parser);
    m_Parser.parse(m_FileName);

    pb.enumeratePaths(paths);

    unsigned pathNum = 0;
    ExecutionPaths::iterator pit;
    for(pit = paths.begin(); pit != paths.end(); ++pit) {
        statsFile << "========== Path " << pathNum << " ========== "<<std::endl;
        PathBuilder::printPath(*pit, std::cout);

        ModuleCache mc(&pb, &m_Library);
        CacheProfiler cp(&mc, &pb);
        TestCase tc(&pb);
        InstructionCounter ic(&pb);

        //Process all the items of the path
        //This will automatically maintain all the module info
        pb.processPath(*pit);

        ic.printCounter(statsFile);
        tc.printInputs(statsFile);

        TopMissesPerModule tmpm(&m_binaries, &cp);

        tmpm.setHtml(HtmlOutput != 0);
        tmpm.setFilteredProcess(CPFilterProcess);
        tmpm.setFilteredModule(CPFilterModule);
        tmpm.setMinMissThreshold(CPMinMissCountFilter);

#if 0
        if (HtmlOutput) {
            cp.printAggregatedStatisticsHtml(statsFile);
        }else {
            cp.printAggregatedStatistics(statsFile);
        }
#endif

        tmpm.computeStats();
        statsFile << "Total misses on this path: " << std::dec << tmpm.getTotalMisses() << std::endl;

        tmpm.printAggregatedStatistics(statsFile);
        tmpm.print(statsFile);

        if (ic.getCount() > maxICount) {
            maxICount = ic.getCount();
            maxICountPath = pathNum;
        }

        if (ic.getCount() < minICount) {
            minICount = ic.getCount();
            minICountPath = pathNum;
        }

        if (tmpm.getTotalMisses() > maxMissCount) {
            maxMissCount = tmpm.getTotalMisses();
            maxMissPath = pathNum;
        }

        if (tmpm.getTotalMisses() < minMissCount) {
            minMissCount = tmpm.getTotalMisses();
            minMissPath = pathNum;
        }


        ++pathNum;
        statsFile << std::endl;
        std::cout << "---------" << std::endl;
    }


    statsFile << "----------------------------------" << std::endl << std::dec;
    statsFile << "Miss count        - Max:" << std::setw(10) << maxMissCount << " (path " << std::setw(10) << maxMissPath << ") ";
    statsFile << "Min:" << std::setw(10)<< minMissCount << "(path " << std::setw(10) << minMissPath << ")" << std::endl;

    statsFile << "Instruction count - Max:" << std::setw(10) << maxICount << " (path "<< std::setw(10) << maxICountPath << ") ";
    statsFile << "Min:"<< std::setw(10) << minICount << "(path " << std::setw(10) << minICountPath << ")" << std::endl;

    return;

}

}

static std::vector<std::string> split(const std::string &s)
{
    std::vector<std::string> ret;
    size_t p, prevp = 0;
    while ((p = s.find(' ')) != std::string::npos) {
        ret.push_back(s.substr(prevp, p));
        prevp = p;
    }
    return ret;
}

int main(int argc, char **argv)
{
    cl::ParseCommandLineOptions(argc, (char**) argv, " pfprofiler\n");
    std::cout << TraceFile << std::endl;
    std::cout << ModPath << std::endl;

    ModList = split(ModListRaw);

    if (AnaType == "cache") {
       PfProfiler pf(TraceFile.getValue());
       pf.process();
    }else if (AnaType == "icount") {
       PfProfiler pf(TraceFile.getValue());
       pf.icountStats();
   }else  if (AnaType == "pf") {
       PfProfiler pf(TraceFile.getValue());
       pf.pageFaults();
   }else {
       std::cout << "Unknown analysis type " << AnaType << std::endl;
   }

    return 0;
}


#ifdef afasfd
#warning Compiling with memory debugging support...

void *operator new(size_t s)
{
    void *ret = malloc(s);
    if (!ret) {
        return NULL;
    }

    memset(ret, 0xAA, s);
    return ret;
}

void* operator new[](size_t s) {
    void *ret = malloc(s);
    if (!ret) {
        return NULL;
    }

    memset(ret, 0xAA, s);
    return ret;
}



void operator delete( void *pvMem )
{
   size_t s =  malloc_usable_size(pvMem);
   memset(pvMem, 0xBB, s);
   free(pvMem);
}

void operator delete[](void *pvMem) {
    size_t s =  malloc_usable_size(pvMem);
    memset(pvMem, 0xBB, s);
    free(pvMem);
}

#endif
