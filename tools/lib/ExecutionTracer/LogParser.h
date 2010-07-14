#ifndef S2ETOOLS_EXECTRACER_LOGPARSER_H
#define S2ETOOLS_EXECTRACER_LOGPARSER_H

#include <string>
#include <sigc++/sigc++.h>
#include <s2e/Plugins/ExecutionTracers/TraceEntries.h>
#include <stdio.h>
#include <vector>

namespace s2etools
{


class LogEvents
{
public:
    sigc::signal<void,
        unsigned,
        const s2e::plugins::ExecutionTraceItemHeader &,
        void *
    >onEachItem;
protected:
    virtual void processItem(unsigned itemEntry,
                             const s2e::plugins::ExecutionTraceItemHeader &hdr,
                             void *data);
};


class LogParser: public LogEvents
{
private:

    //FILE *m_File;
    void *m_File;
    uint64_t m_size;
    std::vector<uint64_t> m_ItemOffsets;


public:
    LogParser();
    ~LogParser();

    bool parse(const std::string &file);
    bool getItem(unsigned index, s2e::plugins::ExecutionTraceItemHeader &hdr, void **data);

};

}

#endif
