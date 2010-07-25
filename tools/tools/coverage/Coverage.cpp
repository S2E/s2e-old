//#define __STDC_CONSTANT_MACROS 1
//#define __STDC_LIMIT_MACROS 1
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
#include <inttypes.h>
#include "Coverage.h"

using namespace llvm;
using namespace s2etools;


namespace {


cl::opt<std::string>
    TraceFile("trace", cl::desc("Input trace"), cl::init("ExecutionTracer.dat"));

cl::opt<std::string>
    LogFile("outputdir", cl::desc("Store the coverage into the given folder"), cl::init("."));

cl::opt<std::string>
    ModPath("modpath", cl::desc("Path to module descriptors"), cl::init("."));

//cl::opt<std::string>
//    CovType("covtype", cl::desc("Coverage type"), cl::init("basicblock"));


}

namespace s2etools
{
BasicBlockCoverage::BasicBlockCoverage(const std::string &basicBlockListFile,
           const std::string &moduleName)
{
    FILE *fp = fopen(basicBlockListFile.c_str(), "r");
    if (!fp) {
        std::cerr << "Could not open file " << basicBlockListFile << std::endl;
        return;
    }

    char buffer[512];
    while (fgets(buffer, sizeof(buffer), fp)) {
        uint64_t start, end;
        char name[512];
        sscanf(buffer, "0x%"PRIx64" 0x%"PRIx64" %[^ ]s", &start, &end, buffer);
        std::cout << "Read 0x" << std::hex << start << " 0x" << end << " " << name << std::endl;

        BasicBlocks &bbs = m_functions[name];
        bbs.insert(BasicBlock(start, end));
        m_allBbs.insert(BasicBlock(start, end));
    }


}

//Start and end must be local to the modle
bool BasicBlockCoverage::addTranslationBlock(uint64_t ts, uint64_t start, uint64_t end)
{
    Block tb(ts, start, end);
    Blocks::iterator it = m_uniqueTbs.find(tb);

    if (it == m_uniqueTbs.end()) {
        m_uniqueTbs.insert(tb);
        return true;
    }else {
        if ((*it).timeStamp > ts) {
            m_uniqueTbs.erase(*it);
            m_uniqueTbs.insert(tb);
            return true;
        }
    }

    return false;
}

void BasicBlockCoverage::convertTbToBb()
{
    BasicBlocks::iterator it;

    for (it = m_allBbs.begin(); it != m_allBbs.end(); ++it) {
        const BasicBlock &bb = *it;
        Block ftb(0, bb.start, 0);

        Blocks::iterator tbit = m_uniqueTbs.find(ftb);
        //basic block was not covered
        if (tbit == m_uniqueTbs.end()) {
            continue;
        }

        Block tb = *tbit;
        if (bb.start == tb.start && bb.end == tb.end) {
            //Basic block matches translation block
            continue;
        }

        assert(tb.start == bb.start && tb.end > bb.end);

        //Erase the tb, and split it
        m_uniqueTbs.erase(tb);
        Block tb1(tb);
        Block tb2(tb);

        //XXX: timestamps
        tb1.end = bb.end;
        tb2.start = bb.end + 1;

        m_uniqueTbs.insert(tb1);
        m_uniqueTbs.insert(tb2);
    }
}


void BasicBlockCoverage::printTimeCoverage(std::ostream &os) const
{
    BlocksByTime bbtime;
    BlocksByTime::const_iterator tit;

    Blocks::const_iterator it;
    for (it = m_uniqueTbs.begin(); it != m_uniqueTbs.end(); ++it) {
        bbtime.insert(*it);
    }

    for (tit = bbtime.begin(); tit != bbtime.end(); ++tit) {
        const Block &b = *tit;
        os << std::dec << b.timeStamp << std::hex << " 0x" << b.start << " 0x" << b.end << std::endl;
    }
}

Coverage::Coverage(BFDLibrary *lib, ModuleCache *cache, LogEvents *events, std::ostream &os) : m_os(os)
{
    m_events = events;
    m_connection = events->onEachItem.connect(
            sigc::mem_fun(*this, &Coverage::onItem)
            );
    m_cache = cache;
    m_library = lib;
}

Coverage::~Coverage()
{
    m_connection.disconnect();

    BbCoverageMap::iterator it;
    for (it = m_bbCov.begin(); it != m_bbCov.end(); ++it) {
        delete (*it).second;
    }
}

void Coverage::onItem(unsigned traceIndex,
            const s2e::plugins::ExecutionTraceItemHeader &hdr,
            void *item)
{
    if (hdr.type != s2e::plugins::TRACE_TB_START) {
        return;
    }

    const s2e::plugins::ExecutionTraceTb *te =
            (const s2e::plugins::ExecutionTraceTb*) item;

    const ModuleInstance *mi = m_cache->getInstance(hdr.pid, te->pc);
    assert(mi);

    BasicBlockCoverage *bbcov = NULL;
    BbCoverageMap::iterator it = m_bbCov.find(mi->Mod->getModuleName());
    if (it == m_bbCov.end()) {
        //Look for the file containing the bbs.
        std::string bblist = mi->Mod->getModuleName() + ".bblist";
        std::string path;
        if (m_library->findLibrary(bblist, path)) {
            BasicBlockCoverage *bb = new BasicBlockCoverage(path, mi->Mod->getModuleName());
            m_bbCov[mi->Mod->getModuleName()] = bb;
            bbcov = bb;
        }
    }else {
        bbcov = (*it).second;
    }

    assert(bbcov);

    bbcov->addTranslationBlock(hdr.timeStamp, te->pc, te->pc+te->size-1);
}


CoverageTool::CoverageTool()
{
    m_library.setPath(ModPath);
    m_binaries.setPath(ModPath);
}

CoverageTool::~CoverageTool()
{

}

#if 0
void CoverageTool::process()
{
    ExecutionPaths paths;
    std::ofstream logfile;
    logfile.open(LogFile.c_str());

    PathBuilder pb(&m_parser);
    m_parser.parse(TraceFile);

    pb.enumeratePaths(paths);

    PathBuilder::printPaths(paths, std::cout);

    ExecutionPaths::iterator pit;

    unsigned pathNum = 0;
    for(pit = paths.begin(); pit != paths.end(); ++pit) {

        std::cout << "Analyzing path " << pathNum << std::endl;
        ModuleCache mc(&pb, &m_library);

        //MemoryDebugger md(&m_binaries, &mc, &pb, logfile);
        //md.lookForValue(MemoryValue);

        Coverage cov(&m_binaries, &mc, &pb, logfile);

        pb.processPath(*pit);
        ++pathNum;
    }
}
#endif

void CoverageTool::flatTrace()
{
    std::ofstream logfile;
    logfile.open(LogFile.c_str());

    ModuleCache mc(&m_parser, &m_library);

    //MemoryDebugger md(&m_binaries, &mc, &pb, logfile);
    //md.lookForValue(MemoryValue);

    Coverage cov(&m_binaries, &mc, &m_parser, logfile);

    m_parser.parse(TraceFile);

}


}


int main(int argc, char **argv)
{
    cl::ParseCommandLineOptions(argc, (char**) argv, " coverage");

    s2etools::CoverageTool cov;
    cov.flatTrace();

    return 0;
}

