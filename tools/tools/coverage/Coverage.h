#ifndef S2ETOOLS_COVERAGE_H
#define S2ETOOLS_COVERAGE_H

#include <lib/ExecutionTracer/LogParser.h>
#include <lib/ExecutionTracer/ModuleParser.h>

#include <lib/BinaryReaders/Library.h>

#include <inttypes.h>
#include <ostream>
#include <set>
#include <map>
#include <string>

namespace s2etools
{

struct BasicBlock
{
    uint64_t start;
    uint64_t end;
    bool operator()(const BasicBlock&b1, const BasicBlock &b2) const {
        return b1.end < b2.start;
    }

    BasicBlock(uint64_t s, uint64_t e) {
        start = s;
        end = e;
    }

    BasicBlock() {
        start = end = 0;
    }
};

//Either a BB or TB depending on the context
struct Block
{
    uint64_t timeStamp;
    uint64_t start;
    uint64_t end;

    bool operator()(const Block&b1, const Block &b2) const {
        return b1.start < b2.start;
    }

    Block() {
        timeStamp = start = end = 0;
    }

    Block(uint64_t ts, uint64_t s, uint64_t e) {
        timeStamp = ts;
        start = s;
        end = e;
    }

    struct SortByTime {

        bool operator()(const Block&b1, const Block &b2) const {
            if (b1.timeStamp < b2.timeStamp) {
                return true;
            }
            return b1.start < b2.start;
        }
    };
};

class BasicBlockCoverage
{
public:

    typedef std::set<BasicBlock, BasicBlock> BasicBlocks;
    typedef std::set<Block, Block> Blocks;
    typedef std::set<Block, Block::SortByTime> BlocksByTime;
    typedef std::map<std::string, BasicBlocks> Functions;


private:
    std::string m_name;
    BasicBlocks m_allBbs;
    Functions m_functions;

    Functions m_coveredFunctions;
    Blocks m_uniqueTbs;
public:
    BasicBlockCoverage(const std::string &basicBlockListFile,
                   const std::string &moduleName);

    //Start and end must be local to the module
    //Returns true if the added block resulted in covering new basic blocks
    bool addTranslationBlock(uint64_t ts, uint64_t start, uint64_t end);

    void convertTbToBb();
    void printTimeCoverage(std::ostream &os) const;
    void printReport(std::ostream &os) const;


};

class Coverage: public LogEvents
{
public:

private:
    LogEvents *m_events;
    ModuleCache *m_cache;
    BFDLibrary *m_library;

    sigc::connection m_connection;

    typedef std::map<std::string, BasicBlockCoverage*> BbCoverageMap;

    BbCoverageMap m_bbCov;


    void onItem(unsigned traceIndex,
                const s2e::plugins::ExecutionTraceItemHeader &hdr,
                void *item);

public:
    Coverage(BFDLibrary *lib, ModuleCache *cache, LogEvents *events);
    virtual ~Coverage();

    void outputCoverage(const std::string &Path) const;

};

class CoverageTool
{
private:
    LogParser m_parser;

    ModuleLibrary m_library;
    BFDLibrary m_binaries;



public:
    CoverageTool();
    ~CoverageTool();

    void process();
    void flatTrace();
};


}

#endif
