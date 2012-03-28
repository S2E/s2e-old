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

#ifndef S2E_PLUGINS_TRACEENTRIES_H
#define S2E_PLUGINS_TRACEENTRIES_H

#include <inttypes.h>
#include <string.h>
#include <string>
#include <vector>

#undef EAX
#undef EBX
#undef ECX
#undef EDX
#undef EDI
#undef ESI
#undef EBP
#undef ESP

namespace s2e {
namespace plugins {

enum ExecTraceEntryType {
    TRACE_MOD_LOAD = 0,
    TRACE_MOD_UNLOAD,
    TRACE_PROC_UNLOAD,
    TRACE_CALL,
    TRACE_RET,
    TRACE_TB_START,
    TRACE_TB_END,
    TRACE_MODULE_DESC,
    TRACE_FORK,
    TRACE_CACHESIM,
    TRACE_TESTCASE,
    TRACE_BRANCHCOV,
    TRACE_MEMORY,
    TRACE_PAGEFAULT,
    TRACE_TLBMISS,
    TRACE_ICOUNT,
    TRACE_MEM_CHECKER,
    TRACE_MAX
};


struct ExecutionTraceItemHeader{
    uint64_t timeStamp;
    uint32_t  size;  //Size of the payload
    uint8_t  type;
    uint32_t stateId;
    uint64_t pid;
    //uint8_t  payload[];
}__attribute__((packed));

struct ExecutionTraceModuleLoad {
    char name[32];
    uint64_t loadBase;
    uint64_t nativeBase;
    uint64_t size;
}__attribute__((packed));

struct ExecutionTraceModuleUnload {
    uint64_t loadBase;
}__attribute__((packed));

struct ExecutionTraceProcessUnload {

}__attribute__((packed));


struct ExecutionTraceCall {
    //These are absolute addresses
    uint64_t source, target;
}__attribute__((packed));

struct ExecutionTraceReturn {
    //These are absolute addresses
    uint64_t source, target;
}__attribute__((packed));

struct ExecutionTraceFork {
    uint64_t pc;
    uint32_t stateCount;
    //Array of states (uint32_t)...
    uint32_t children[1];
}__attribute__((packed));

struct ExecutionTraceBranchCoverage {
    uint64_t pc;
    uint64_t destPc;
}__attribute((packed));

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
}__attribute__((packed));

struct ExecutionTraceCacheSimName {
    uint8_t type;
    uint32_t id;
    //XXX: make sure it does not overflow the overall entry size
    uint32_t length;
    uint8_t  name[1];

    //XXX: should use placement new operator instead
    static ExecutionTraceCacheSimName *allocate(
            uint32_t id, const std::string &str, uint32_t *retsize) {
        unsigned size = sizeof(ExecutionTraceCacheSimName) +
                        str.size();
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

}__attribute__((packed));

struct ExecutionTraceCacheSimEntry {
    uint8_t type;
    uint8_t cacheId;
    uint64_t pc, address;
    uint8_t size;
    uint8_t isWrite, isCode, missCount;
    //XXX: should find a compact way of encoding caches.
}__attribute__((packed));

union ExecutionTraceCache {
    uint8_t type;
    ExecutionTraceCacheSimParams params;
    ExecutionTraceCacheSimName name;
    ExecutionTraceCacheSimEntry entry;
}__attribute__((packed));

struct ExecutionTraceMemChecker
{
    enum Flags{
        GRANT=1, REVOKE=2,
        READ=4, WRITE=8, EXECUTE=16,
        RESOURCE=32
    };

    struct Serialized {
        uint64_t start;
        uint32_t size;
        uint32_t flags;
        uint32_t nameLength;
    }__attribute__((packed));

    uint64_t start;
    uint32_t size;
    Flags flags;
    std::string name;

    static Serialized *serialize(unsigned *serializedSize,
                                 uint64_t regionStart, uint32_t regionSize,
                                 Flags flags, const std::string &name) {
        unsigned bufsize = sizeof(Serialized)
                + name.size();

        uint8_t *a = new uint8_t[bufsize];
        Serialized *ret = (Serialized*)a;

        ret->flags = (uint32_t)flags;
        ret->start = regionStart;
        ret->size = regionSize;
        ret->nameLength = name.length();

        for (unsigned i=0; i<name.length(); ++i) {
            (a + sizeof(Serialized))[i] = name[i];
        }

        *serializedSize = bufsize;
        return ret;
    }

    static void deserialize(const Serialized *in, ExecutionTraceMemChecker *out) {
        out->start = in->start;
        out->size = in->size;
        out->flags = (Flags)in->flags;

        uint8_t *str = (uint8_t*)(in+1);
        for (unsigned i=0; i<in->nameLength; ++i) {
            out->name += str[i];
        }
    }

};


struct ExecutionTraceTestCase {
    struct Header {
        uint32_t nameSize;
        uint32_t dataSize;
    }__attribute__((packed));

    typedef std::pair<std::string, std::vector<unsigned char> > VarValuePair;
    typedef std::vector<VarValuePair> ConcreteInputs;
    static ExecutionTraceTestCase *serialize(unsigned *size, const ConcreteInputs &inputs) {
        unsigned bufsize=0;
        ConcreteInputs::const_iterator it;
        for(it = inputs.begin(); it != inputs.end(); ++it) {
            bufsize += sizeof(Header) + (*it).first.size() + (*it).second.size();
        }

        uint8_t *a = new uint8_t[bufsize];
        ExecutionTraceTestCase *ret = (ExecutionTraceTestCase*)a;

        for(it = inputs.begin(); it != inputs.end(); ++it) {
            Header hdr = {(*it).first.size(), (*it).second.size()};
            memcpy(a, &hdr, sizeof(hdr));
            a+=sizeof(hdr);
            memcpy(a, (*it).first.c_str(), (*it).first.size());
            a+=(*it).first.size();
            for (unsigned i=0; i<(*it).second.size(); ++i, ++a) {
                *a = (*it).second[i];
            }
        }
        *size = bufsize;
        return ret;
    }

    static void deserialize(void *buf, size_t buflen, ConcreteInputs &out) {
        uint8_t *a = (uint8_t*)buf;
        while(buflen > 0) {
            Header *hdr = (Header*)a;
            a+=sizeof(*hdr);
            buflen -= sizeof(*hdr);
            out.push_back(VarValuePair("", std::vector<unsigned char>()));
            VarValuePair &vp = out.back();
            for (unsigned i=0; i<hdr->nameSize; ++i, a++, --buflen) {
                vp.first += (char) *a;
            }
            for (unsigned i=0; i<hdr->dataSize; ++i, a++, --buflen) {
                vp.second.push_back((char) *a);
            }
        }
    }

    static void deallocate(ExecutionTraceTestCase *o) {
        delete [] (uint8_t *)o;
    }


};

#define EXECTRACE_MEM_WRITE 1
#define EXECTRACE_MEM_IO    2
#define EXECTRACE_MEM_SYMBVAL 4
#define EXECTRACE_MEM_SYMBADDR 8
#define EXECTRACE_MEM_HASHOSTADDR 16
#define EXECTRACE_MEM_SYMBHOSTADDR 32
struct ExecutionTraceMemory
{
    uint64_t pc;
    uint64_t address;
    uint64_t value;
    uint8_t  size;
    uint8_t  flags;

    //The next field is written only if  EXECTRACE_MEM_HASHOST is set!
    uint64_t hostAddress;
}__attribute__((packed));

struct ExecutionTracePageFault
{
    uint64_t pc;
    uint64_t address;
    uint8_t isWrite;
}__attribute__((packed));

struct ExecutionTraceTlbMiss
{
    uint64_t pc;
    uint64_t address;
    uint8_t isWrite;
}__attribute__((packed));

struct ExecutionTraceICount
{
    //How many instructions where executed so far.
    uint64_t count;
}__attribute__((packed));

//XXX: Avoid hard-coded registers
//XXX: Extend to other kinds of registers
struct ExecutionTraceTb
{
    enum ETranslationBlockType
    {
        TB_DEFAULT=0,
        TB_JMP, TB_JMP_IND,
        TB_COND_JMP, TB_COND_JMP_IND,
        TB_CALL, TB_CALL_IND, TB_REP, TB_RET
    };

    enum EX86Registers
    {
         EAX=0,
         ECX=1,
         EDX=2,
         EBX=3,
         ESP=4,
         EBP=5,
         ESI=6,
         EDI=7
    };

    uint64_t pc, targetPc;
    uint32_t size;
    uint8_t tbType;

    uint8_t symbMask;
    uint32_t registers[8];
}__attribute__((packed));

union ExecutionTraceAll {
    ExecutionTraceModuleLoad moduleLoad;
    ExecutionTraceModuleUnload moduleUnload;
    ExecutionTraceCall call;
    ExecutionTraceReturn ret;
}__attribute__((packed));

}
}
#endif
