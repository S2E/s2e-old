#include <s2e/plugins/ExecutionTracers/TraceEntries.h>
#include <cassert>
#include <stack>
#include <ostream>
#include <iostream>
#include "Path.h"

namespace s2etools
{

PathSegment::PathSegment(PathSegment *parent, uint32_t stateId, uint64_t forkPc)
{
    m_StateId = stateId;
    m_ForkPc = forkPc;
    m_Parent = NULL;

    if (parent) {
        m_Parent = parent;
        PathSegmentList::const_iterator it;
        for(it = m_Parent->m_Children.begin();
            it != m_Parent->m_Children.end(); ++it) {
            //The parent can appear only once in the set of its children
            assert ((*it)->m_StateId != stateId);
        }
        m_Parent->m_Children.push_back(this);
    }
}

unsigned PathSegment::getIndexInParent() const
{
    if (!m_Parent) {
        return 0;
    }

    const PathSegmentList &c = m_Parent->getChildren();
    PathSegmentList::const_iterator it;
    unsigned i=0;
    for (it = c.begin(); it != c.end(); ++it) {
        if ((*it) == this) {
            return i;
        }
        ++i;
    }
    assert(false);
    return 0;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

PathBuilder::PathBuilder(LogParser *log)
{
    m_Parser = log;

    log->onEachItem.connect(
            sigc::mem_fun(*this, &PathBuilder::onItem)
    );

    m_Root = new PathSegment(NULL, 0, 0);
    m_CurrentSegment = m_Root;
    m_Leaves[0].push_back(m_CurrentSegment);
}

void PathBuilder::onItem(unsigned traceIndex,
            const s2e::plugins::ExecutionTraceItemHeader &hdr,
            void *item)
{
    assert(m_CurrentSegment);
    if (hdr.stateId != m_CurrentSegment->getStateId()) {
        //Lookup the current state
        StateToSegments::iterator it = m_Leaves.find(hdr.stateId);
        //There must have been a fork that generated the state
        if (it == m_Leaves.end()) {
            std::cerr << (int) hdr.stateId << std::endl;
            assert(false);
        }
        assert((*it).second.size() > 0);
        m_CurrentSegment = (*it).second.back();
    }

    if (hdr.type == s2e::plugins::TRACE_FORK) {
        s2e::plugins::ExecutionTraceFork *f = (s2e::plugins::ExecutionTraceFork*)item;
        for(unsigned i = 0; i<f->stateCount; ++i) {
            PathSegment *newSeg = new PathSegment(m_CurrentSegment, f->children[i], f->pc);
            m_Leaves[f->children[i]].push_back(newSeg);
        }
    }else {
        //Simply extend the current segment with a fragment
        if (!m_CurrentSegment->hasFragments()) {
            m_CurrentSegment->appendFragment(PathFragment(traceIndex, traceIndex));
        }else {
            m_CurrentSegment->expandLastFragment(traceIndex);
        }
    }
}


void PathBuilder::enumeratePaths(ExecutionPaths &paths)
{
    ExecutionPath currentPath;
    std::stack<PathSegment*> s;

    s.push(m_Root);

    while(s.size()>0) {
        PathSegment *curSeg = s.top();
        s.pop();

        const PathSegmentList &children = curSeg->getChildren();
        PathSegmentList::const_iterator it;
        if (children.size() > 0) {
            for (it = children.begin(); it != children.end(); ++it) {
                s.push(*it);
            }
        }else {
            //We have finished traversing one path, build it.
            paths.push_back(ExecutionPath());
            ExecutionPath &curPath = paths.back();

            //curSeg = curSeg->getParent();
            while(curSeg->getParent() != 0) {
                curPath.push_back(curSeg->getIndexInParent());
                curSeg = curSeg->getParent();
            }
        }
    }
}

void PathBuilder::printPath(const ExecutionPath &p, std::ostream &os)
{
    ExecutionPath::const_reverse_iterator it;
    for (it = p.rbegin(); it != p.rend(); ++it) {
        os << (*it) << " ";
    }
    os << std::endl;
}

void PathBuilder::printPaths(const ExecutionPaths &p, std::ostream &os)
{
    ExecutionPaths::const_iterator it;
    for (it = p.begin(); it!=p.end(); ++it) {
        printPath(*it, os);
    }
}

void PathBuilder::processSegment(PathSegment *seg)
{
    const PathFragmentList &fra = seg->getFragmentList();
    PathFragmentList::const_iterator it;
    s2e::plugins::ExecutionTraceItemHeader hdr;
    uint8_t data[256];

    for (it = fra.begin(); it != fra.end(); ++it) {
        const PathFragment &f = (*it);
        std::cout << "frag(" << f.startIndex << "," << f.endIndex << ")"<< std::endl;
        for (uint32_t s = f.startIndex; s <= f.endIndex; ++s) {
            m_Parser->getItem(s, hdr, data);
            processItem(s, hdr, data);
        }
    }
}

void PathBuilder::processPath(const ExecutionPath &p)
{
    ExecutionPath::const_reverse_iterator it;
    PathSegment *curSeg = m_Root;

    it = p.rbegin();

    do {
        processSegment(curSeg);

        if (it == p.rend()) {
            break;
        }

        const PathSegmentList &ps = curSeg->getChildren();
        if (!(*it < ps.size())) {
            std::cerr << *it << " - " << ps.size() << std::endl;
            assert(false);
        }

        curSeg = ps[*it];
        ++it;
    }while(true);
}

}
