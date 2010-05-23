#ifndef S2E_PLUGINS_TRACEENTRIES_H
#define S2E_PLUGINS_TRACEENTRIES_H

#include <inttypes.h>

namespace s2e {
namespace plugins {

enum ExecTraceEntryType {
    TRACE_MOD_LOAD = 0,
    TRACE_MOD_UNLOAD,
    TRACE_CALL, TRACE_RET,
    TRACE_TB_START,
    TRACE_TB_END,
    TRACE_MODULE_DESC
};


struct ExecutionTraceItemHeader{
    uint64_t timeStamp;
    uint8_t  size;  //Size of the payload
    uint8_t  type;
    uint32_t stateId;
    uint64_t pid;
    uint8_t  payload[];
};

struct ExecutionTraceModuleLoad {
    char name[32];
    uint64_t loadBase;
    uint64_t nativeBase;
    uint64_t size;
};

struct ExecutionTraceModuleUnload {
    uint32_t traceEntry;
};


struct ExecutionTraceCall {
    uint32_t moduleId;

    //These are absolute addresses
    uint64_t source, target;
};

struct ExecTraceRet {

};


}
}
#endif
