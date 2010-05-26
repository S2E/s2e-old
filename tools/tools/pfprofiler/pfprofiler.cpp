#define __STDC_CONSTANT_MACROS 1
#define __STDC_LIMIT_MACROS 1
#define __STDC_FORMAT_MACROS 1


#include "llvm/Support/CommandLine.h"
#include <stdio.h>
#include <inttypes.h>
#include <iostream>

#include <lib/ExecutionTracer/ModuleParser.h>
#include <lib/ExecutionTracer/Path.h>

#include "pfprofiler.h"

using namespace llvm;
using namespace s2etools;
using namespace s2e::plugins;

namespace {

cl::opt<std::string>
    TraceFile("trace", cl::desc("<input trace>"), cl::init("ExecutionTracer.dat"));

cl::opt<std::string>
        ModPath("modpath", cl::desc("Path to module descriptors"), cl::init("."));
}



///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void CacheParameters::print(std::ostream &os)
{
    os << std::dec;
    os << "Cache " << m_name << " - Statistics" << std::endl;
    os << "Total Read  Misses: " << m_TotalMissesOnRead << std::endl;
    os << "Total Write Misses: " << m_TotalMissesOnWrite << std::endl;
    os << "Total       Misses: " << m_TotalMissesOnRead + m_TotalMissesOnWrite << std::endl;
}

CacheProfiler::CacheProfiler(LogEvents *events)
{
    m_Events = events;
    onEachItem.connect(
            sigc::mem_fun(*this, &CacheProfiler::onItem)
            );
}

CacheProfiler::~CacheProfiler()
{
    Caches::iterator it;
    for (it = m_caches.begin(); it != m_caches.end(); ++it) {
        delete (*it).second;
    }
}

void CacheProfiler::processCacheItem(const ExecutionTraceCacheSimEntry *e)
{
    Caches::iterator it = m_caches.find(e->cacheId);
    assert(it != m_caches.end());

    CacheParameters *c = (*it).second;

    if (e->missCount > 0) {
        if (e->isWrite) {
            c->m_TotalMissesOnWrite += e->missCount;
        }else {
            c->m_TotalMissesOnRead += e->missCount;
        }
    }
}

void CacheProfiler::onItem(unsigned traceIndex,
            const s2e::plugins::ExecutionTraceItemHeader &hdr,
            void *item)
{
    if (hdr.type != TRACE_CACHESIM) {
        return;
    }

    ExecutionTraceCache *e = (ExecutionTraceCache*)item;
    if (e->type == CACHE_NAME) {
        std::string s((const char*)e->name.name, e->name.length);
        m_cacheIds[e->name.id] = s;
    }else if (e->type == CACHE_PARAMS) {
        CacheIdToName::iterator it = m_cacheIds.find(e->params.cacheId);
        assert(it != m_cacheIds.end());

        CacheParameters *params = new CacheParameters((*it).second, e->params.lineSize, e->params.size,
                                                      e->params.associativity);

        m_caches[e->params.cacheId] = params;
        //XXX: fix that when needed
        //params->setUpperCache(NULL);
    }else if (e->type == CACHE_ENTRY) {
        const ExecutionTraceCacheSimEntry *se = &e->entry;
        processCacheItem(se);
    }else {
        assert(false && "Unknown cache trace entry");
    }
}

void CacheProfiler::printAggregatedStatistics(std::ostream &os) const
{
    Caches::const_iterator it;

    for(it = m_caches.begin(); it != m_caches.end(); ++it) {
        (*it).second->print(os);
        os << "-------------------------------------" << std::endl;
    }
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////




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

    ExecutionPaths paths;
    PathBuilder pb(&m_Parser);
    m_Parser.parse(m_FileName);

    pb.enumeratePaths(paths);

    unsigned pathNum = 0;
    ExecutionPaths::iterator pit;
    for(pit = paths.begin(); pit != paths.end(); ++pit) {
        CacheProfiler cp(&pb);
        pb.processPath(*pit);
        std::cout << "========== Path " << pathNum << std::endl;
        cp.printAggregatedStatistics(std::cout);
        ++pathNum;
        std::cout << std::endl;
    }

    PathBuilder::printPaths(paths, std::cout);

    return;

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


