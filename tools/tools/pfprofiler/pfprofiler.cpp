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

    if (!m_Library.get(load.name)) {
        std::string modFile = ModPath + "/";
        modFile += load.name;
        modFile += ".fcn";
        Module *mod = ModuleParser::parseTextDescription(modFile);
        if (mod) {
            mod->print(std::cout);
            m_Library.addModule(mod);
            assert(m_Library.get(load.name));
        }
    }


    if (!m_ModuleCache->loadDriver(load.name, hdr.pid, load.loadBase, load.nativeBase, load.size)) {
        std::cout << "Could not load driver " << load.name << std::endl;
    }
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
    m_ModuleCache = new ModuleCache(&m_Library);

    m_Parser.onCallItem.connect(
            sigc::mem_fun(*this, &PfProfiler::processCallItem)
    );

    m_Parser.onModuleLoadItem.connect(
            sigc::mem_fun(*this, &PfProfiler::processModuleLoadItem)
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


