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
#include <stdio.h>
#include <inttypes.h>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>

#include <lib/ExecutionTracer/ModuleParser.h>
#include <lib/ExecutionTracer/Path.h>
#include <lib/ExecutionTracer/TestCase.h>
#include <lib/ExecutionTracer/PageFault.h>
#include <lib/ExecutionTracer/InstructionCounter.h>
#include <lib/BinaryReaders/BFDInterface.h>
#include <lib/Utils/BasicBlockListParser.h>


#include "llvm/Support/Path.h"

#include "TbTrace.h"


using namespace llvm;
using namespace s2etools;
using namespace s2e::plugins;


namespace {

cl::list<std::string>
    TraceFiles("trace", llvm::cl::value_desc("Input trace"), llvm::cl::Prefix,
               llvm::cl::desc("Specify an execution trace file"));

cl::opt<std::string>
    LogDir("outputdir", cl::desc("Store the list of translation blocks into the given folder"), cl::init("."));

cl::list<std::string>
    ModDir("moddir", cl::desc("Directory containing the binary modules"));

cl::list<unsigned>
    PathList("pathId",
             cl::desc("Path id to output, repeat for more. Empty=all paths"), cl::ZeroOrMore);

cl::opt<bool>
        PrintRegisters("printRegisters", cl::desc("Print register contents for each block"), cl::init(false));

cl::opt<bool>
        PrintMemory("printMemory", cl::desc("Print memory trace"), cl::init(false));

cl::opt<bool>
        PrintDisassembly("printDisassembly", cl::desc("Print disassembly in the trace"), cl::init(false));

cl::opt<bool>
        PrintMemoryChecker("printMemoryChecker", cl::desc("Print memory checker events"), cl::init(false));

cl::opt<bool>
        PrintMemoryCheckerStack("printMemoryCheckerStack", cl::desc("Print stack grants/revocations"), cl::init(false));


}

namespace s2etools
{

TbTrace::TbTrace(Library *lib, ModuleCache *cache, LogEvents *events, std::ofstream &of)
    :m_output(of)
{
    m_events = events;
    m_connection = events->onEachItem.connect(
            sigc::mem_fun(*this, &TbTrace::onItem)
            );
    m_cache = cache;
    m_library = lib;
    m_hasItems = false;
    m_hasDebugInfo = false;
    m_hasModuleInfo = false;
}

TbTrace::~TbTrace()
{
    m_connection.disconnect();
}

bool TbTrace::parseDisassembly(const std::string &listingFile, Disassembly &out)
{
    //Get the module name
    llvm::sys::Path listingFilePath(listingFile);
    std::string moduleName = llvm::sys::path::stem(listingFilePath.str());

    bool added = false;

    char line[1024];
    std::filebuf file;
    if (!file.open(listingFile.c_str(), std::ios::in)) {
        return false;
    }

    std::istream is(&file);

    while(is.getline(line, sizeof(line))) {
        std::string ln = line;
        //Skip the start
        unsigned i=0;
        while (line[i] && (line[i]!=':'))
            ++i;
        if (line[i] != ':')
            continue;

        ++i;
        //Grab the address
        std::string strpc;
        while(isxdigit(line[i])) {
            strpc = strpc + line[i];
            ++i;
        }

        uint64_t pc=0;
        sscanf(strpc.c_str(), "%"PRIx64, &pc);
        if (pc) {
            out[moduleName][pc].push_back(ln);
            added = true;
        }
    }

    return added;
}

void TbTrace::printDisassembly(const std::string &module, uint64_t relPc, unsigned tbSize)
{
    Disassembly::iterator it = m_disassembly.find(module);
    if (it == m_disassembly.end()) {
        llvm::sys::Path disassemblyListing;
        if (!m_library->findDisassemblyListing(module, disassemblyListing)) {
            std::cerr << "Could not find disassembly listing for module "
                         << module << std::endl;
            return;
        }

        if (!parseDisassembly(disassemblyListing.str(), m_disassembly)) {
            return;
        }
        it = m_disassembly.find(module);
        assert(it != m_disassembly.end());
    }

    //Fetch the basic blocks for our module
    ModuleBasicBlocks::iterator bbit = m_basicBlocks.find(module);
    if (bbit == m_basicBlocks.end()) {
        llvm::sys::Path basicBlockList;

        if (!m_library->findBasicBlockList(module, basicBlockList)) {
            std::cerr << "TbTrace: could not find basic block list for  "
                      << module << std::endl;
            exit(-1);
        }

        TbTraceBbs moduleBbs;

        if (!BasicBlockListParser::parseListing(basicBlockList, moduleBbs)) {
            std::cerr << "TbTrace: could not parse basic block list in file "
                      << basicBlockList.str() << std::endl;
            exit(-1);
        }

        m_basicBlocks[module] = moduleBbs;
        bbit = m_basicBlocks.find(module);
        assert(bbit != m_basicBlocks.end());
    }


    while((int)tbSize > 0) {
        //Fetch the right basic block
        BasicBlock bbToFetch(relPc, 1);
        TbTraceBbs::iterator mybb = (*bbit).second.find(bbToFetch);
        if (mybb == (*bbit).second.end()) {
            m_output << "Could not find basic block 0x" << std::hex << relPc << " in the list" << std::endl;
            return;
        }

        //Found the basic block, compute the range of program counters
        //whose disassembly we are going to print.
        const BasicBlock &bb = *mybb;
        uint64_t asmStartPc = relPc;
        uint64_t asmEndPc;

        if (relPc + tbSize >= bb.start + bb.size) {
            asmEndPc = bb.start + bb.size;
        }else {
            asmEndPc = relPc + tbSize;
        }

        assert(relPc >= bb.start && relPc < bb.start + bb.size);


        //Grab the vector of strings for the program counter
        ModuleDisassembly::iterator modIt = (*it).second.find(asmStartPc);
        if (modIt == (*it).second.end()) {
            return;
        }

        //Fetch the range of program counters from the disassembly file
        ModuleDisassembly::iterator modItEnd = (*it).second.lower_bound(asmEndPc);

        for (ModuleDisassembly::iterator it = modIt; it != modItEnd; ++it) {
            //Print the vector we've got
            for(DisassemblyEntry::const_iterator asmIt = (*it).second.begin();
                asmIt != (*it).second.end(); ++asmIt) {
                m_output << "\033[1;33m" << *asmIt << "\033[0m" << std::endl;
            }
        }

        tbSize -= asmEndPc - asmStartPc;
        relPc += asmEndPc - asmStartPc;

        if ((int)tbSize < 0) {
            assert(false && "Cannot be negative");
        }
    }

}

void TbTrace::printDebugInfo(uint64_t pid, uint64_t pc, unsigned tbSize, bool printListing)
{
    ModuleCacheState *mcs = static_cast<ModuleCacheState*>(m_events->getState(m_cache, &ModuleCacheState::factory));
    const ModuleInstance *mi = mcs->getInstance(pid, pc);
    if (!mi) {
        return;
    }
    uint64_t relPc = pc - mi->LoadBase + mi->ImageBase;
    m_output << std::hex << "(" << mi->Name;
    if (relPc != pc) {
       m_output << " 0x" << relPc;
    }
    m_output << ")";

    m_hasModuleInfo = true;

    std::string file = "?", function="?";
    uint64_t line=0;
    if (m_library->getInfo(mi, pc, file, line, function)) {
        size_t pos = file.find_last_of('/');
	if (pos != std::string::npos) {
            file = file.substr(pos+1);
        }

        m_output << " " << file << std::dec << ":" << line << " in " << function;
        m_hasDebugInfo = true;
    }

    if (PrintDisassembly && printListing) {
        m_output << std::endl;
        printDisassembly(mi->Name, relPc, tbSize);
    }
}

void TbTrace::printRegisters(const s2e::plugins::ExecutionTraceTb *te)
{
    const char *regs[] = {"EAX", "ECX", "EDX", "EBX", "ESP", "EBP", "ESI", "EDI"};
    for (unsigned i=0; i<8; ++i) {
        if (te->symbMask & (1<<i)) {
            m_output << regs[i] << ": SYMBOLIC ";
        }else {
            m_output << regs[i] << ": 0x" << std::hex << te->registers[i] << " ";
        }
    }
}

void TbTrace::printMemoryChecker(const s2e::plugins::ExecutionTraceMemChecker::Serialized *item)
{
    ExecutionTraceMemChecker deserializedItem;

    ExecutionTraceMemChecker::deserialize(item, &deserializedItem);


    bool isStackItem = deserializedItem.name.find("stack") != deserializedItem.name.npos;
    if (!PrintMemoryCheckerStack && isStackItem) {
        return;
    }

    m_output << "\033[1;31mMEMCHECKER\033[0m";

    std::string nameHighlightCode;

    if (deserializedItem.flags & ExecutionTraceMemChecker::REVOKE) {
        nameHighlightCode = "\033[1;31m";
        m_output << nameHighlightCode << " REVOKE ";

    }

    if (deserializedItem.flags & ExecutionTraceMemChecker::GRANT) {
        nameHighlightCode = "\033[1;32m";
        m_output << nameHighlightCode << " GRANT  ";
    }

    if (deserializedItem.flags & ExecutionTraceMemChecker::READ) {
        m_output << " READ   ";
    }

    if (deserializedItem.flags & ExecutionTraceMemChecker::WRITE) {
        m_output << " WRITE  ";
    }



    m_output << nameHighlightCode << deserializedItem.name << "\033[0m";

    m_output << " address=0x" << std::hex << deserializedItem.start
             << " size=0x" << deserializedItem.size << std::endl;
}

void TbTrace::onItem(unsigned traceIndex,
            const s2e::plugins::ExecutionTraceItemHeader &hdr,
            void *item)
{
    //m_output << "Trace index " << std::dec << traceIndex << std::endl;
    if (hdr.type == s2e::plugins::TRACE_MOD_LOAD) {
        const s2e::plugins::ExecutionTraceModuleLoad &load = *(s2e::plugins::ExecutionTraceModuleLoad*)item;
        m_output << "Loaded module " << load.name
                 << " at 0x" << std::hex << load.loadBase;
        m_output << std::endl;
        return;
    }

    if (hdr.type == s2e::plugins::TRACE_MOD_UNLOAD) {
        const s2e::plugins::ExecutionTraceModuleUnload &unload = *(s2e::plugins::ExecutionTraceModuleUnload*)item;
        m_output << "Unloaded module at 0x" << unload.loadBase;
        m_output << std::endl;
        return;
    }

    if (hdr.type == s2e::plugins::TRACE_FORK) {
        s2e::plugins::ExecutionTraceFork *f = (s2e::plugins::ExecutionTraceFork*)item;
        m_output << "Forked at 0x" << std::hex << f->pc << " - ";
        printDebugInfo(hdr.pid, f->pc, 0, false);
        m_output << std::endl;
        return;
    }

    if (hdr.type == s2e::plugins::TRACE_TB_START) {
        const s2e::plugins::ExecutionTraceTb *te =
                (const s2e::plugins::ExecutionTraceTb*) item;

        m_output << "0x" << std::hex << te->pc<< " - ";

        if (PrintRegisters) {
            m_output << std::endl << "    ";
            printRegisters(te);
            m_output << std::endl << "    ";
        }

        printDebugInfo(hdr.pid, te->pc, te->size, true);

        m_output << std::endl;
        m_hasItems = true;
        return;
    }

    if (PrintMemory && (hdr.type == s2e::plugins::TRACE_MEMORY)) {
        const s2e::plugins::ExecutionTraceMemory *te =
                (const s2e::plugins::ExecutionTraceMemory*) item;
        std::string type;

        type += te->flags & EXECTRACE_MEM_SYMBHOSTADDR ? "H" : "h";
        type += te->flags & EXECTRACE_MEM_SYMBADDR ? "A" : "a";
        type += te->flags & EXECTRACE_MEM_SYMBVAL ? "S" : "C";
        type += te->flags & EXECTRACE_MEM_WRITE   ? "W" : "R";
        m_output << "S=" << std::dec << hdr.stateId << " P=0x" << std::hex << hdr.pid << " PC=0x" << std::hex << te->pc << " " << type << (int)te->size << "[0x"
                << std::hex << te->address << "]=0x" << std::setw(10) << std::setfill('0') << te->value;

	if (te->flags & EXECTRACE_MEM_HASHOSTADDR) {
           m_output << " hostAddr=0x" << te->hostAddress << " ";
        }
        m_output << "\t";

        printDebugInfo(hdr.pid, te->pc, 0, false);
        m_output << std::setfill(' ');
        m_output << std::endl;
       return;
    }

    if (PrintMemoryChecker && (hdr.type == s2e::plugins::TRACE_MEM_CHECKER)) {
        const s2e::plugins::ExecutionTraceMemChecker::Serialized *te =
                (const s2e::plugins::ExecutionTraceMemChecker::Serialized*) item;
        printMemoryChecker(te);
    }
}

TbTraceTool::TbTraceTool()
{
    m_binaries.setPaths(ModDir);
}

TbTraceTool::~TbTraceTool()
{

}

void TbTraceTool::flatTrace()
{
    PathBuilder pb(&m_parser);
    m_parser.parse(TraceFiles);

    ModuleCache mc(&pb);
    TestCase tc(&pb);

    PathSet paths;
    pb.getPaths(paths);

    cl::list<unsigned>::const_iterator listit;

    if (PathList.empty()) {
        PathSet::iterator pit;
        for (pit = paths.begin(); pit != paths.end(); ++pit) {
            PathList.push_back(*pit);
        }
    }

    //XXX: this is efficient only for short paths or for a small number of
    //path, because complexity is O(n2): we reprocess the prefixes.
    for(listit = PathList.begin(); listit != PathList.end(); ++listit) {
        std::cout << "Processing path " << std::dec << *listit << std::endl;
        PathSet::iterator pit = paths.find(*listit);
        if (pit == paths.end()) {
            std::cerr << "Could not find path with id " << std::dec <<
                    *listit << " in the execution trace." << std::endl;
            continue;
        }

        std::stringstream ss;
        ss << LogDir << "/" << *listit << ".txt";
        std::ofstream traceFile(ss.str().c_str());

        TbTrace trace(&m_binaries, &mc, &pb, traceFile);

        if (!pb.processPath(*listit)) {
            std::cerr << "Could not process path " << std::dec << *listit << std::endl;
            continue;
        }

        traceFile << "----------------------" << std::endl;

        if (trace.hasDebugInfo() == false) {
            traceFile << "WARNING: No debug information for any module in the path " << std::dec << *listit << std::endl;
            traceFile << "WARNING: Make sure you have set the module path properly and the binaries contain debug information."
                    << std::endl << std::endl;
        }

        if (trace.hasModuleInfo() == false) {
            traceFile << "WARNING: No module information for any module in the path " << std::dec << *listit << std::endl;
            traceFile << "WARNING: Make sure to use the ModuleTracer plugin before running this tool."
                    << std::endl << std::endl;
        }

        if (trace.hasItems() == false ) {
            traceFile << "WARNING: No basic blocks in the path " << std::dec << *listit << std::endl;
            traceFile << "WARNING: Make sure to use the TranslationBlockTracer plugin before running this tool. "
                    << std::endl << std::endl;
        }

        TestCaseState *tcs = static_cast<TestCaseState*>(pb.getState(&tc, *pit));
        if (!tcs) {
            traceFile << "WARNING: No test case in the path " << std::dec << *listit << std::endl;
            traceFile << "WARNING: Make sure to use the TestCaseGenerator plugin and terminate the states before running this tool. "
                    << std::endl << std::endl;
        }else {
            tcs->printInputs(traceFile);
        }
    }

}

}

int main(int argc, char **argv)
{
    cl::ParseCommandLineOptions(argc, (char**) argv, " tbtrace");

    s2etools::TbTraceTool trace;
    trace.flatTrace();

    return 0;
}

