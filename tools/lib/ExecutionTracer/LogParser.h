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

#ifndef S2ETOOLS_EXECTRACER_LOGPARSER_H
#define S2ETOOLS_EXECTRACER_LOGPARSER_H

#include <string>
#include <lib/Utils/Signals/Signals.h>
#include <s2e/Plugins/ExecutionTracers/TraceEntries.h>
#include <stdio.h>
#include <vector>
#include <map>
#include <set>

#ifdef _WIN32
#include <windows.h>
#endif

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

    struct LogFile {
        #ifdef _WIN32
        HANDLE m_hFile;
        HANDLE m_hMapping;
        #endif
        void *m_File;
        uint64_t m_size;

        LogFile() {
            #ifdef _WIN32
            m_hFile = NULL;
            m_hMapping = NULL;
            #endif
            m_File = NULL;
            m_size = 0;
        }
    };

    typedef std::vector<LogFile> LogFiles;

    LogFiles m_files;
    std::vector<uint8_t*> m_ItemAddresses;

    ItemProcessors m_ItemProcessors;
    void *m_cachedProcessor;
    ItemProcessorState* m_cachedState;

protected:


public:
    LogParser();
    virtual ~LogParser();

    bool parse(const std::vector<std::string> fileNames);
    bool parse(const std::string &file);
    bool getItem(unsigned index, s2e::plugins::ExecutionTraceItemHeader &hdr, void **data);

    virtual ItemProcessorState* getState(void *processor, ItemProcessorStateFactory f);
    virtual ItemProcessorState* getState(void *processor, uint32_t pathId);
    virtual void getPaths(PathSet &s);
};

}

#endif
