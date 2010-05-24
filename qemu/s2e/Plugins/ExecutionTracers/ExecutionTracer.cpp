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
            return 0;
        }
    }

    return ++m_CurrentIndex;
}



} // namespace plugins
} // namespace s2e
