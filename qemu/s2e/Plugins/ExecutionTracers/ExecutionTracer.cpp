#include "ExecutionTracer.h"

#include <s2e/S2E.h>
#include <s2e/ConfigFile.h>
#include <s2e/Utils.h>

#include <llvm/System/TimeValue.h>

#include <iostream>

namespace s2e {
namespace plugins {

S2E_DEFINE_PLUGIN(ExecutionTracer, "ExecutionTracer plugin", "",);

void ExecutionTracer::initialize()
{

    m_LogFile = fopen(s2e()->getOutputFilename("ExecutionTracer.dat").c_str(), "wb");
    assert(m_LogFile);
    m_CurrentIndex = 0;

    s2e()->getCorePlugin()->onStateFork.connect(
            sigc::mem_fun(*this, &ExecutionTracer::onFork));

}

ExecutionTracer::~ExecutionTracer()
{
    fclose(m_LogFile);
}

uint32_t ExecutionTracer::writeData(
        S2EExecutionState *state,
        void *data, unsigned size, ExecTraceEntryType type)
{
    ExecutionTraceItemHeader item;

    assert(size < 256);
    assert(data);
    item.timeStamp = llvm::sys::TimeValue::now().usec();
    item.size = size;
    item.type = type;
    item.stateId = state->getID();
    item.pid = state->getPid();

    if (fwrite(&item, sizeof(item), 1, m_LogFile) != 1) {
        return 0;
    }

    if (size) {
        if (fwrite(data, size, 1, m_LogFile) != 1) {
            //at this point the log is corrupted.
            assert(false);
        }
    }

    return ++m_CurrentIndex;
}

void ExecutionTracer::onFork(S2EExecutionState *state,
            const std::vector<S2EExecutionState*>& newStates,
            const std::vector<klee::ref<klee::Expr> >& newConditions
            )
{
    assert(newStates.size() > 0);

    unsigned itemSize = sizeof(ExecutionTraceFork) +
                        (newStates.size()-1) * sizeof(uint32_t);

    uint8_t *itemBytes = new uint8_t[itemSize];
    ExecutionTraceFork *itemFork = reinterpret_cast<ExecutionTraceFork*>(itemBytes);

    itemFork->pc = state->getPc();
    itemFork->stateCount = newStates.size();


    for (unsigned i=0; i<newStates.size(); i++) {
        itemFork->children[i] = newStates[i]->getID();
    }

    writeData(state, itemFork, itemSize, TRACE_FORK);

    delete [] itemBytes;
}


} // namespace plugins
} // namespace s2e
