#ifndef S2E_PLUGINS_TRACEENTRIES_H
#define S2E_PLUGINS_TRACEENTRIES_H

#include <inttypes.h>
#include <string.h>
#include <string>

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
    TRACE_FORK,
    TRACE_CACHESIM
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
    uint64_t pc;
    uint32_t stateCount;
    //Array of states (uint32_t)...
    uint32_t children[1];
};


enum CacheSimDescType{
    CACHE_PARAMS=0,
    CACHE_NAME,
    CACHE_ENTRY
};

struct ExecutionTraceCacheSimParams {
    uint8_t type;
    uint32_t cacheId;
    uint32_t size;
    uint32_t lineSize;
    uint32_t associativity;
    uint32_t upperCacheId;
};

struct ExecutionTraceCacheSimName {
    uint8_t type;
    uint32_t id;
    //XXX: make sure it does not overflow the overall entry size
    uint32_t length;
    uint8_t  name[0];

    //XXX: should use placement new operator instead
    static ExecutionTraceCacheSimName *allocate(
            uint32_t id, const std::string &str, uint32_t *retsize) {
        unsigned size = sizeof(ExecutionTraceCacheSimName) +
                        str.size() + 1;
        uint8_t *a = new uint8_t[size];
        ExecutionTraceCacheSimName *ret = (ExecutionTraceCacheSimName*)a;
        strcpy((char*)ret->name, str.c_str());
        ret->length = str.size();
        ret->id = id;
        ret->type = CACHE_NAME;
        *retsize = size;
        return ret;
    }
    static void deallocate(ExecutionTraceCacheSimName *o) {
        delete [] (uint8_t *)o;
    }

};

struct ExecutionTraceCacheSimEntry {
    uint8_t type;
    uint8_t cacheId;
    uint64_t pc, address;
    uint8_t size;
    uint8_t isWrite, isCode, missCount;
    //XXX: should find a compact way of encoding caches.
};

union ExecutionTraceCache {
    uint8_t type;
    ExecutionTraceCacheSimParams params;
    ExecutionTraceCacheSimName name;
    ExecutionTraceCacheSimEntry entry;
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
