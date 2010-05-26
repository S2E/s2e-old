#ifndef S2E_PLUGINS_TRACEENTRIES_H
#define S2E_PLUGINS_TRACEENTRIES_H

#include <inttypes.h>

namespace s2e {
namespace plugins {

enum ExecTraceEntryType {
    TRACE_MOD_LOAD = 0,
    TRACE_MOD_UNLOAD,
    TRACE_PROC_UNLOAD,
    TRACE_CALL, TRACE_RET,
    TRACE_TB_START,
    TRACE_TB_END,
    TRACE_MODULE_DESC,
    TRACE_FORK
};


struct ExecutionTraceItemHeader{
    uint64_t timeStamp;
    uint8_t  size;  //Size of the payload
    uint8_t  type;
    uint32_t stateId;
    uint64_t pid;
    //uint8_t  payload[];
};

struct ExecutionTraceModuleLoad {
    char name[32];
    uint64_t loadBase;
    uint64_t nativeBase;
    uint64_t size;
};

struct ExecutionTraceModuleUnload {
    uint64_t loadBase;
};

struct ExecutionTraceProcessUnload {

};


struct ExecutionTraceCall {
    //These are absolute addresses
    uint64_t source, target;
};

struct ExecutionTraceReturn {
    //These are absolute addresses
    uint64_t source, target;
};

struct ExecutionTraceFork {
    uint32_t pc;
    uint32_t stateCount;
    //Array of states (uint32_t)...
    uint32_t children[1];
};


union ExecutionTraceAll {
    ExecutionTraceModuleLoad moduleLoad;
    ExecutionTraceModuleUnload moduleUnload;
    ExecutionTraceCall call;
    ExecutionTraceReturn ret;
};

}
}
#endif
