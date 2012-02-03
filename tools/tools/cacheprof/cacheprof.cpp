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
#include <lib/ExecutionTracer/CacheProfiler.h>
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



//Print aggregated number of cache misses for all caches
void printGlobalCacheStats(PathBuilder &pb,
                           CacheProfiler &cprof,
                           PathSet &paths,
                           TestCase &testCase)
{
    std::string outFileStr = LogDir + "/cacheprof.log";
    std::ofstream outFile(outFileStr.c_str());

    outFile << "#Aggregated number of caches misses over the entire paths" << std::endl;
    outFile << "#PathId CacheMisses TestCase" << std::endl;

    PathSet::const_iterator pit;

    uint32_t minPath = (uint32_t)-1, maxPath = 0;
    uint64_t minCount = (uint64_t)-1, maxCount = 0;
    uint64_t totalCount = 0, totalCount2 = 0;
    uint64_t completedPath = 0;

    for(pit = paths.begin(); pit != paths.end(); ++pit) {
        outFile << std::dec << *pit << ": ";

        ItemProcessorState *state = pb.getState(&cprof, *pit);
        CacheProfilerState *cps = static_cast<CacheProfilerState*>(state);

        uint64_t missCount = (cps->m_globalStats.readMissCount + cps->m_globalStats.writeMissCount);

        outFile << missCount;

        state = pb.getState(&testCase, *pit);
        TestCaseState *testCaseState = static_cast<TestCaseState*>(state);
        if (testCaseState) {
            outFile << " T:";
            testCaseState->printInputsLine(outFile);
        } else {
            outFile << " No test cases in the trace";
        }

        outFile << std::endl;

        if (testCaseState) {
            ++completedPath;
            totalCount += missCount;
            totalCount2 += missCount*missCount;

            if (minCount > missCount) {
                minCount = missCount;
                minPath = *pit;
            }

            if (maxCount < missCount) {
                maxCount = missCount;
                maxPath = *pit;
            }
        }
    }

    uint64_t average = totalCount / completedPath;
    uint64_t variance = totalCount2 / completedPath - average*average;

    outFile << "#The following applies to paths that have a test case" << std::endl;
    outFile << "#MaxMissCount: " << maxCount << " path: " << maxPath << std::endl;
    outFile << "#MinMissCount: " << minCount << " path: " << minPath << std::endl;
    outFile << "#AverageCount: " << average << std::endl;
    outFile << "#Stdev:        " << sqrt(variance) << std::endl;
    outFile << "#CompletedPaths: " << completedPath << std::endl;
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

    CacheProfiler cprof(&pb);
    TestCase testCase(&pb);

    pb.processTree();

    PathSet paths;
    pb.getPaths(paths);


    printGlobalCacheStats(pb, cprof, paths, testCase);

    return 0;
}

