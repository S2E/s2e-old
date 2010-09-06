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
#include <sstream>
#include <inttypes.h>
#include <iomanip>
#include "Coverage.h"

using namespace llvm;
using namespace s2etools;


namespace {


cl::opt<std::string>
    TraceFile("trace", cl::desc("Input trace"), cl::init("ExecutionTracer.dat"));

cl::opt<std::string>
    LogDir("outputdir", cl::desc("Store the coverage into the given folder"), cl::init("."));

cl::opt<std::string>
    ModPath("modpath", cl::desc("Path to module descriptors"), cl::init("."));

cl::opt<bool>
    Compact("compact", cl::desc("Do not display non-covered blocks"), cl::init(false));


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
        sscanf(buffer, "0x%"PRIx64" 0x%"PRIx64" %[^\r\t\n]s", &start, &end, name);
        //std::cout << "Read 0x" << std::hex << start << " 0x" << end << " " << name << std::endl;

        BasicBlocks &bbs = m_functions[name];
        //XXX: the +1 is here to compensate the broken extraction script, which
        //does not take into account the whole size of the last instruction.
        bbs.insert(BasicBlock(start, end));
        m_allBbs.insert(BasicBlock(start, end));

    }

    Functions::iterator fit;
    unsigned fcnBbCount = 0;
    for (fit = m_functions.begin(); fit != m_functions.end() ; ++fit) {
        fcnBbCount += (*fit).second.size();
    }
    assert(fcnBbCount == m_allBbs.size());

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
    Blocks::iterator tbit;

    for(tbit = m_uniqueTbs.begin(); tbit != m_uniqueTbs.end(); ++tbit) {
        const Block &tb = *tbit;

        for (uint64_t s = tb.start; s < tb.end; s++) {

            BasicBlock tbb(s, s+1);
            it = m_allBbs.find(tbb);
            if(it == m_allBbs.end()) {
                std::cerr << "Missing TB: " << std::hex << "0x"
                    << tb.start << ":0x" << tb.end << std::endl;
                continue;
            }
            //assert(it != m_allBbs.end());

            BasicBlock newBlock;
            newBlock.timeStamp = tb.timeStamp;
            newBlock.start = (*it).start;
            newBlock.end = (*it).end;

            if (m_coveredBbs.find(newBlock) == m_coveredBbs.end()) {
                    m_coveredBbs.insert(newBlock);
            }

        }

    }

}


void BasicBlockCoverage::printTimeCoverage(std::ostream &os) const
{
    BlocksByTime bbtime;
    BlocksByTime::const_iterator tit;

    bool timeInited = false;
    uint64_t firstTime = 0;

    BasicBlocks::const_iterator it;
    for (it = m_coveredBbs.begin(); it != m_coveredBbs.end(); ++it) {
        bbtime.insert(*it);
    }

    for (tit = bbtime.begin(); tit != bbtime.end(); ++tit) {
        const BasicBlock &b = *tit;

        if (!timeInited) {
            firstTime = b.timeStamp;
            timeInited = true;
        }

        os << std::dec << (b.timeStamp - firstTime)/1000000 << std::hex << " 0x" << b.start << " 0x" << b.end << std::endl;
    }
}

void BasicBlockCoverage::printReport(std::ostream &os) const
{
    unsigned touchedFunctions = 0;
    unsigned fullyCoveredFunctions = 0;
    unsigned touchedFunctionsBb = 0;
    unsigned touchedFunctionsTotalBb = 0;
    Functions::const_iterator fit;

    for(fit = m_functions.begin(); fit != m_functions.end(); ++fit) {
        const BasicBlocks &fcnbb = (*fit).second;
        BasicBlocks uncovered;
        BasicBlocks::const_iterator bbit;
        for (bbit = fcnbb.begin(); bbit != fcnbb.end(); ++bbit) {
            if (m_coveredBbs.find(*bbit) == m_coveredBbs.end()) {
                uncovered.insert(*bbit);
            }
        }

        unsigned int coveredCount = (fcnbb.size() - uncovered.size());
        char line[512];
        snprintf(line, sizeof(line), "(%03u%%) %03u/%03u %-50s ",
                 (unsigned)(coveredCount*100/fcnbb.size()), coveredCount, (unsigned)fcnbb.size(),
                 (*fit).first.c_str());

        os << line;

        if (uncovered.size() == fcnbb.size()) {
            os << "The function was not exercised";
        }else {
            if (uncovered.size() == 0) {
                os << "Full coverage";
                fullyCoveredFunctions++;
            }else {
                if (!Compact) {
                    for (bbit = uncovered.begin(); bbit != uncovered.end(); ++bbit) {
                        os << std::hex << "0x" << (*bbit).start << " ";
                    }
                }
            }
            touchedFunctionsBb += coveredCount;
            touchedFunctionsTotalBb += fcnbb.size();
            touchedFunctions++;
        }

        os << std::endl;

    }
    os << std::endl;

    os << "Basic block coverage:    " << std::dec << m_coveredBbs.size() << "/" << m_allBbs.size() <<
            "(" << (m_coveredBbs.size()*100/m_allBbs.size()) << "%)"  << std::endl;


    os << "Function block coverage: " << std::dec << touchedFunctionsBb << "/" << touchedFunctionsTotalBb <<
            "(" << (touchedFunctionsBb*100/touchedFunctionsTotalBb) << "%)"  << std::endl;


    os << "Total touched functions: " << std::dec << touchedFunctions << "/" << m_functions.size() <<
            "(" << (touchedFunctions*100/m_functions.size()) << "%)"  << std::endl;

    os << "Fully covered functions: " << std::dec << fullyCoveredFunctions << "/" << m_functions.size() <<
            "(" << (fullyCoveredFunctions*100/m_functions.size()) << "%)"  << std::endl;

}

void BasicBlockCoverage::printBBCov(std::ostream &os) const
{
    unsigned touchedFunctions = 0;
    unsigned fullyCoveredFunctions = 0;
    unsigned touchedFunctionsBb = 0;
    unsigned touchedFunctionsTotalBb = 0;
    Functions::const_iterator fit;

    for(fit = m_functions.begin(); fit != m_functions.end(); ++fit) {
        const BasicBlocks &fcnbb = (*fit).second;
        BasicBlocks uncovered;
        BasicBlocks::const_iterator bbit;
        for (bbit = fcnbb.begin(); bbit != fcnbb.end(); ++bbit) {
            Block b(0, (*bbit).start, 0);
            if (m_uniqueTbs.find(b) == m_uniqueTbs.end())
                os << std::setw(0) << "-";
            else 
                os << std::setw(0) << "+";

            os << std::hex << "0x" << std::setfill('0') << std::setw(8) << (*bbit).start << std::setw(0)
                   << ":0x" << std::setw(8) << (*bbit).end << std::endl;
        }
        os << std::endl;

    }
}

Coverage::Coverage(Library *lib, ModuleCache *cache, LogEvents *events)
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

    ModuleCacheState *mcs = static_cast<ModuleCacheState*>(m_events->getState(m_cache, &ModuleCacheState::factory));

    const ModuleInstance *mi = mcs->getInstance(hdr.pid, te->pc);
    if (!mi) {
        std::cerr << "Could not find module for pc=0x" << std::hex << te->pc << std::endl;
        return;
    }

    BasicBlockCoverage *bbcov = NULL;
    BbCoverageMap::iterator it = m_bbCov.find(mi->Name);
    if (it == m_bbCov.end()) {
        //Look for the file containing the bbs.
        std::string bblist = mi->Name + ".bblist";
        std::string path;
        if (m_library->findLibrary(bblist, path)) {
            BasicBlockCoverage *bb = new BasicBlockCoverage(path, mi->Name);
            m_bbCov[mi->Name] = bb;
            bbcov = bb;
        }
    }else {
        bbcov = (*it).second;
    }

    assert(bbcov);

    uint64_t relPc = te->pc - mi->LoadBase + mi->ImageBase;

    bbcov->addTranslationBlock(hdr.timeStamp, relPc, relPc+te->size-1);
}

void Coverage::outputCoverage(const std::string &path) const
{
    BbCoverageMap::const_iterator it;

    for(it = m_bbCov.begin(); it != m_bbCov.end(); ++it) {
        std::stringstream ss;
        ss << path << "/" << (*it).first << ".timecov";
        std::ofstream timecov(ss.str().c_str());

        (*it).second->convertTbToBb();
        (*it).second->printTimeCoverage(timecov);

        std::stringstream ss1;
        ss1 << path << "/" << (*it).first << ".repcov";
        std::ofstream report(ss1.str().c_str());
        (*it).second->printReport(report);

        std::stringstream ss2;
        ss2 << path << "/" << (*it).first << ".bbcov";
        std::ofstream bbcov(ss2.str().c_str());
        (*it).second->printBBCov(bbcov);
    }
}


CoverageTool::CoverageTool()
{
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
    PathBuilder pb(&m_parser);
    m_parser.parse(TraceFile);

    ModuleCache mc(&pb);
    Coverage cov(&m_binaries, &mc, &pb);

    pb.processTree();

    cov.outputCoverage(LogDir);

}


}


int main(int argc, char **argv)
{
    cl::ParseCommandLineOptions(argc, (char**) argv, " coverage");

    s2etools::CoverageTool cov;
    cov.flatTrace();

    return 0;
}

