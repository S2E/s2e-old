#define __STDC_CONSTANT_MACROS 1
#define __STDC_LIMIT_MACROS 1
#define __STDC_FORMAT_MACROS 1


#include "llvm/Support/CommandLine.h"
#include <stdio.h>
#include <inttypes.h>
#include <iostream>

#include "pfprofiler.h"

using namespace llvm;

namespace {

cl::opt<std::string>
    TraceFile(cl::desc("<input bytecode>"), cl::Positional, cl::init("ExecutionTracer.dat"));

}

PfProfiler::PfProfiler(const std::string &file)
{
    m_FileName = file;
}

PfProfiler::~PfProfiler()
{

}

void PfProfiler::processCallItem(unsigned traceIndex,
                     const s2e::plugins::ExecutionTraceItemHeader &hdr,
                     const s2e::plugins::ExecutionTraceCall &call)
{
    printf("Processing entry %d - (%d) %#"PRIx64" -> %#"PRIx64"\n", traceIndex,
           call.moduleId, call.source, call.target);
}

void PfProfiler::process()
{
    m_Parser.onCallItem.connect(
            sigc::mem_fun(*this, &PfProfiler::processCallItem)
    );
    m_Parser.parse(m_FileName);
}

int main(int argc, char **argv)
{
    cl::ParseCommandLineOptions(argc, (char**) argv, " pfprofiler\n");
    std::cout << TraceFile << std::endl;

    PfProfiler pf(TraceFile.getValue());
    pf.process();

    return 0;
}


