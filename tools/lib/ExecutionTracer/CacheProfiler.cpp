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

#include <iomanip>
#include <iostream>
#include <cassert>
#include "CacheProfiler.h"

using namespace s2e::plugins;

namespace s2etools {

CacheProfiler::CacheProfiler(LogEvents *events)
{
   m_events = events;
   m_connection = events->onEachItem.connect(
           sigc::mem_fun(*this, &CacheProfiler::onItem));
}

CacheProfiler::~CacheProfiler()
{
    m_connection.disconnect();
}

void CacheProfiler::onItem(unsigned traceIndex,
        const s2e::plugins::ExecutionTraceItemHeader &hdr,
        void *item)
{
    if (hdr.type != s2e::plugins::TRACE_CACHESIM) {
        return;
    }

    ExecutionTraceCache *cacheItem = (ExecutionTraceCache*)item;

    switch(cacheItem->type) {

        //Save the name of the cache and the associated id.
        //Actual parameters will come later in the trace
        case s2e::plugins::CACHE_NAME: {
            std::string s((const char*)cacheItem->name.name, cacheItem->name.length);
            m_cacheIds[cacheItem->name.id] = s;
        }
        break;

        //Create the cache according to the parameters
        //in the trace
        case s2e::plugins::CACHE_PARAMS: {
            CacheIdToName::iterator it = m_cacheIds.find(cacheItem->params.cacheId);
            assert(it != m_cacheIds.end());

            Cache *params = new Cache((*it).second, cacheItem->params.lineSize,
                                      cacheItem->params.size, cacheItem->params.associativity);

            assert(m_caches.find(cacheItem->params.cacheId) == m_caches.end());
            m_caches[cacheItem->params.cacheId] = params;
            //XXX: fix that when needed
            //params->setUpperCache(NULL);
        }
        break;

        case s2e::plugins::CACHE_ENTRY: {
            const ExecutionTraceCacheSimEntry *se = &cacheItem->entry;

            CacheProfilerState *state = static_cast<CacheProfilerState*>(m_events->getState(this, &CacheProfilerState::factory));
            state->processCacheItem(this, hdr, *se);
        }
        break;

        default: {
            assert(false && "Unknown cache trace entry");
        }

    }
}


///////////////////////////////////////////////////////////
ItemProcessorState *CacheProfilerState::factory()
{
    return new CacheProfilerState();
}

CacheProfilerState::CacheProfilerState()
{

}

CacheProfilerState::~CacheProfilerState()
{

}

ItemProcessorState *CacheProfilerState::clone() const
{
    return new CacheProfilerState(*this);
}

void CacheProfilerState::processCacheItem(CacheProfiler *cp,
                      const s2e::plugins::ExecutionTraceItemHeader &hdr,
                      const s2e::plugins::ExecutionTraceCacheSimEntry &e)
{
    CacheProfiler::Caches::iterator it = cp->m_caches.find(e.cacheId);
    assert(it != cp->m_caches.end());

    Cache *c = (*it).second;
    assert(c);

    CacheStatistics addend(e.isWrite ? 0 : e.missCount,
                           e.isWrite ? e.missCount : 0);

    m_globalStats += addend;

#if 0
    //Update the per-state global miss-rate count for the cache
    CacheStatistics &perCache = m_cacheStats[c];

    perCache += addend;
    perCache.c = c;

    //Update per-instruction misses
    CacheStats &instrStats = m_perInstructionStats[std::make_pair(hdr.pid, e.pc)];
    CacheStatistics &instrPerCache = instrStats[c];
    instrPerCache += addend;
    instrPerCache.c = c;
#endif
}

}
