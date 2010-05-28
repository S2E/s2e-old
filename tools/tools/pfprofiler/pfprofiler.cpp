#define __STDC_CONSTANT_MACROS 1
#define __STDC_LIMIT_MACROS 1
#define __STDC_FORMAT_MACROS 1


#include "llvm/Support/CommandLine.h"
#include <stdio.h>
#include <inttypes.h>
#include <iostream>
#include <fstream>

#include <lib/ExecutionTracer/ModuleParser.h>
#include <lib/ExecutionTracer/Path.h>
#include <lib/BinaryReaders/BFDInterface.h>

#include "pfprofiler.h"
#include "CacheProfiler.h"

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
        OutFile("outfile", cl::desc("Output file"), cl::init("stats.dat"));

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



PfProfiler::PfProfiler(const std::string &file)
{
    m_FileName = file;
    m_ModuleCache = NULL;

    m_Library.setPath(ModPath);
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

void PfProfiler::process()
{



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
        //PathBuilder::printPath(*pit, std::cout);

        ModuleCache mc(&pb, &m_Library);
        CacheProfiler cp(&mc, &pb);

        //Process all the items of the path
        //This will automatically maintain all the module info
        pb.processPath(*pit);

        TopMissesPerModule tmpm(&cp);

        tmpm.setHtml(HtmlOutput != 0);
        tmpm.setFilteredProcess(CPFilterProcess);
        tmpm.setFilteredModule(CPFilterModule);
        tmpm.setMinMissThreshold(CPMinMissCountFilter);

        if (HtmlOutput) {
            cp.printAggregatedStatisticsHtml(statsFile);
        }else {
            cp.printAggregatedStatistics(statsFile);
        }
        tmpm.computeStats();
        tmpm.printAggregatedStatistics(statsFile);
        tmpm.print(statsFile, ModPath);

        ++pathNum;
        statsFile << std::endl;
        std::cout << "---------" << std::endl;
    }



    return;

    m_ModuleCache = new ModuleCache(&m_Parser, &m_Library);

    m_Parser.onCallItem.connect(
            sigc::mem_fun(*this, &PfProfiler::processCallItem)
    );


    m_Parser.parse(m_FileName);

    m_Library.print(std::cout);
}

int main(int argc, char **argv)
{
    cl::ParseCommandLineOptions(argc, (char**) argv, " pfprofiler\n");
    std::cout << TraceFile << std::endl;
    std::cout << ModPath << std::endl;


    PfProfiler pf(TraceFile.getValue());
    pf.process();

    return 0;
}


