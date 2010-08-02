#ifndef S2ETOOLS_EXECTRACER_LOGPARSER_H
#define S2ETOOLS_EXECTRACER_LOGPARSER_H

#include <string>
#include <sigc++/sigc++.h>
#include <s2e/Plugins/ExecutionTracers/TraceEntries.h>
#include <stdio.h>
#include <vector>
#include <map>
#include <set>

namespace s2etools
{


/**
 *  Trace item processors must use this class if they with to store
 *  aggregated data along trace processing.
 */
class ItemProcessorState
{
public:
    virtual ~ItemProcessorState() {};
    virtual ItemProcessorState *clone() const = 0;
};

//opaque references the registered trace processor
typedef std::map<void *, ItemProcessorState*> ItemProcessors;
typedef std::set<uint32_t> PathSet;

typedef ItemProcessorState* (*ItemProcessorStateFactory)();

class LogEvents
{
public:
    sigc::signal<void,
        unsigned,
        const s2e::plugins::ExecutionTraceItemHeader &,
        void *
    >onEachItem;

    virtual ItemProcessorState* getState(void *processor, ItemProcessorStateFactory f) = 0;
    virtual ItemProcessorState* getState(void *processor, uint32_t pathId) = 0;
    virtual void getPaths(PathSet &s) = 0;

protected:
    virtual void processItem(unsigned itemEntry,
                             const s2e::plugins::ExecutionTraceItemHeader &hdr,
                             void *data);

    LogEvents();
    virtual ~LogEvents();

};



class LogParser: public LogEvents
{
private:

    //FILE *m_File;
    void *m_File;
    uint64_t m_size;
    std::vector<uint64_t> m_ItemOffsets;

    ItemProcessors m_ItemProcessors;
    void *m_cachedProcessor;
    ItemProcessorState* m_cachedState;

protected:


public:
    LogParser();
    virtual ~LogParser();

    bool parse(const std::string &file);
    bool getItem(unsigned index, s2e::plugins::ExecutionTraceItemHeader &hdr, void **data);

    virtual ItemProcessorState* getState(void *processor, ItemProcessorStateFactory f);
    virtual ItemProcessorState* getState(void *processor, uint32_t pathId);
    virtual void getPaths(PathSet &s);
};

}

#endif
