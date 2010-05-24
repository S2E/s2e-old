#define __STDC_CONSTANT_MACROS 1
#define __STDC_LIMIT_MACROS 1
#define __STDC_FORMAT_MACROS 1


#include "llvm/Support/CommandLine.h"
#include <stdio.h>
#include <inttypes.h>
#include <iostream>

#include <lib/ExecutionTracer/ModuleParser.h>

#include "pfprofiler.h"

using namespace llvm;
using namespace s2etools;

namespace {

cl::opt<std::string>
    TraceFile("trace", cl::desc("<input trace>"), cl::init("ExecutionTracer.dat"));

cl::opt<std::string>
        ModPath("modpath", cl::desc("Path to module descriptors"), cl::init("."));
}

PfProfiler::PfProfiler(const std::string &file)
{
    m_FileName = file;
    m_ModuleCache = NULL;
}

PfProfiler::~PfProfiler()
{

}

void PfProfiler::processModuleLoadItem(unsigned traceIndex,
                     const s2e::plugins::ExecutionTraceItemHeader &hdr,
                     const s2e::plugins::ExecutionTraceModuleLoad &load)
{
    std::cout << "Processing entry " << std::dec << traceIndex << " - ";
    std::cout << load.name << " " << std::hex << load.loadBase << " " << load.size << std::endl;
    //printf("Processing entry %d - %s %#"PRIx64"  %#"PRIx64"\n", traceIndex,
    //       load.name, load.loadBase, load.size);

    m_ModuleCache->loadDriver(load.name, hdr.pid, load.loadBase);
}


void PfProfiler::processCallItem(unsigned traceIndex,
                     const s2e::plugins::ExecutionTraceItemHeader &hdr,
                     const s2e::plugins::ExecutionTraceCall &call)
{
    std::cout << "Processing entry " << std::dec << traceIndex << " - ";
    std::cout << std::hex << call.source << " -> " << call.target << std::endl;


    const ModuleInstance *mi = m_ModuleCache->getInstance(hdr.pid, call.source);
    if (mi) {
        mi->print(std::cout);
    }
}

void PfProfiler::process()
{
    Module *mod = ModuleParser::parseTextDescription(ModPath + "pcntpci5.sys.fcn");
    mod->print(std::cout);

    m_Library.addModule(mod);

    assert(m_Library.get("pcntpci5.sys"));

    m_ModuleCache = new ModuleCache(&m_Library);

    m_Parser.onCallItem.connect(
            sigc::mem_fun(*this, &PfProfiler::processCallItem)
    );

    m_Parser.onModuleLoadItem.connect(
            sigc::mem_fun(*this, &PfProfiler::processModuleLoadItem)
    );

    m_Parser.parse(m_FileName);
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


