#include "llvm/Support/CommandLine.h"
#include <stdio.h>
#include <iostream>

using namespace llvm;

namespace {

cl::opt<std::string>
    TraceFile(cl::desc("<input bytecode>"), cl::Positional, cl::init("-"));

}

int main(int argc, char **argv)
{
    cl::ParseCommandLineOptions(argc, (char**) argv, " pfprofiler\n");
    std::cout << TraceFile << std::endl;

    return 0;
}


