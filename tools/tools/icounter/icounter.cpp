/*
 * S2E Selective Symbolic Execution Framework
 *
 * Copyright (c) 2010, Dependable Systems Laboratory, EPFL
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Dependable Systems Laboratory, EPFL nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE DEPENDABLE SYSTEMS LABORATORY, EPFL BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Currently maintained by:
 *    Vitaly Chipounov <vitaly.chipounov@epfl.ch>
 *    Volodymyr Kuznetsov <vova.kuznetsov@epfl.ch>
 *
 * All contributors are listed in S2E-AUTHORS file.
 *
 */

#define __STDC_FORMAT_MACROS 1

#include "llvm/Support/CommandLine.h"

#include <lib/ExecutionTracer/ModuleParser.h>
#include <lib/ExecutionTracer/Path.h>
#include <lib/ExecutionTracer/TestCase.h>
#include <lib/ExecutionTracer/InstructionCounter.h>
#include <lib/BinaryReaders/BFDInterface.h>
#include <lib/BinaryReaders/Library.h>

#include <s2e/Plugins/ExecutionTracers/TraceEntries.h>

#include <stdio.h>
#include <ostream>
#include <fstream>
#include <iostream>
#include <sstream>
#include <inttypes.h>
#include <iomanip>
//#include "icounter.h"

using namespace llvm;
using namespace s2etools;


namespace {

cl::list<std::string>
    TraceFiles("trace", llvm::cl::value_desc("Input trace"), llvm::cl::Prefix,
               llvm::cl::desc("Specify an execution trace file"));

cl::opt<std::string>
    LogDir("outputdir", cl::desc("Store the coverage into the given folder"), cl::init("."));

cl::list<std::string>
    ModPath("modpath", cl::desc("Path to modules"));

}



namespace s2etools
{

#if 0
InstructionCounterTool::InstructionCounterTool(Library *lib, ModuleCache *cache, LogEvents *events)
{
    m_events = events;
    m_connection = events->onEachItem.connect(
            sigc::mem_fun(*this, &ForkProfiler::onItem)
            );
    m_cache = cache;
    m_library = lib;
}

InstructionCounterTool::~InstructionCounterTool()
{
    m_connection.disconnect();
}

void InstructionCounterTool::onItem(unsigned traceIndex,
            const s2e::plugins::ExecutionTraceItemHeader &hdr,
            void *item)
{
    if (hdr.type != s2e::plugins::TRACE_ICOUNT) {
        return;
    }

    const s2e::plugins::ExecutionTraceICount *te =
            (const s2e::plugins::ExecutionTraceICount*) item;



}
#endif

}

int main(int argc, char **argv)
{
    cl::ParseCommandLineOptions(argc, (char**) argv, " debugger");

    Library library;
    library.setPaths(ModPath);

    LogParser parser;
    PathBuilder pb(&parser);
    parser.parse(TraceFiles);

    ModuleCache mc(&pb);

    InstructionCounter icounter(&pb);
    TestCase testCase(&pb);

    pb.processTree();

    PathSet paths;
    PathSet::const_iterator pit;

    pb.getPaths(paths);

    std::string outFileStr = LogDir + "/icount.log";
    std::ofstream outFile(outFileStr.c_str());

    outFile << "#Path ICount TestCase" << std::endl;

    for(pit = paths.begin(); pit != paths.end(); ++pit) {
        outFile << std::dec << *pit << ": ";

        ItemProcessorState *state = pb.getState(&icounter, *pit);
        InstructionCounterState *ics = static_cast<InstructionCounterState*>(state);

        if (ics) {
            uint64_t icount = ics->getCount();
            outFile << std::dec << icount << " ";
        } else {
            outFile << "No instruction count ";
        }


        state = pb.getState(&testCase, *pit);
        TestCaseState *testCaseState = static_cast<TestCaseState*>(state);
        if (testCaseState) {
            testCaseState->printInputsLine(outFile);
        } else {
            outFile << "No test cases in the trace";
        }


        outFile << std::endl;
    }


    return 0;
}

