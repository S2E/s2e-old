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

#ifndef S2ETOOLS_EXECTRACER_PATH_H

#define S2ETOOLS_EXECTRACER_PATH_H

#include <vector>
#include <map>

#include "LogParser.h"

namespace s2etools
{

/**
 *  Represents a contiguous sequence of trace entries belonging to the same state.
 */
struct PathFragment
{
    uint32_t startIndex, endIndex;
    PathFragment(uint32_t s, uint32_t e) {
        startIndex = s;
        endIndex = e;
    }

    void print(std::ostream &os) const {
        os << std::dec << "(" << startIndex << "," << endIndex << ")";
    }
};

/**
 *  Represents a sequence of fragments between to fork point
 */
typedef std::vector<PathFragment> PathFragmentList;

class PathSegment;
typedef std::vector<PathSegment *>PathSegmentList;
typedef std::map<void *, ItemProcessorState*> PathSegmentStateMap;

/**
 *  A path segment is a sequence of fragments terminated by a fork point
 */
class PathSegment
{
private:
    PathSegment *m_Parent;
    uint32_t m_StateId;
    uint64_t m_ForkPc;

    PathFragmentList m_FragmentList;

    /** Pointers to the forked children */
    PathSegmentList m_Children;

    /** Holds the per-trace processor state */
    PathSegmentStateMap m_SegmentState;
public:
    PathSegment(PathSegment *parent, uint32_t stateId, uint64_t forkPc);
    uint32_t getStateId() const {
        return m_StateId;
    }

    ~PathSegment();

    void deleteState();

    void appendFragment(const PathFragment &f) {
        m_FragmentList.push_back(f);
    }

    void expandLastFragment(uint32_t newEnd) {
        assert(m_FragmentList.back().endIndex <= newEnd);
        m_FragmentList.back().endIndex = newEnd;
    }

    bool hasFragments() const {
        return m_FragmentList.size() > 0;
    }

    const PathFragmentList& getFragmentList() const {
        return m_FragmentList;
    }

    const PathSegmentList& getChildren() const {
        return m_Children;
    }

    PathSegmentStateMap& getStateMap() {
        return m_SegmentState;
    }

    unsigned getIndexInParent() const;

    PathSegment *getParent() const {
        return m_Parent;
    }

    void print(std::ostream &os) const;

};

typedef std::map<uint32_t, PathSegmentList> StateToSegments;

//Sequence of indexes in the children set
typedef std::vector<uint32_t> ExecutionPath;
typedef std::vector<ExecutionPath> ExecutionPaths;




class PathBuilder: public LogEvents
{
private:
    PathSegment *m_Root;
    PathSegment *m_CurrentSegment;
    StateToSegments m_Leaves;
    LogParser *m_Parser;
    sigc::connection m_connection;

    void onItem(unsigned traceIndex,
                const s2e::plugins::ExecutionTraceItemHeader &hdr,
                void *item);

    void processSegment(PathSegment *seg);
public:
    PathBuilder(LogParser *log);
    ~PathBuilder();

    //The paths are inverted!
    void enumeratePaths(ExecutionPaths &paths);

    static void printPath(const ExecutionPath &p, std::ostream &os);
    static void printPaths(const ExecutionPaths &p, std::ostream &os);

    bool processPath(uint32_t);
    void processTree();

    void resetTree();
    virtual ItemProcessorState* getState(void *processor, ItemProcessorStateFactory f);
    virtual ItemProcessorState* getState(void *processor, uint32_t pathId);
    virtual void getPaths(PathSet &s);
};

}

#endif
