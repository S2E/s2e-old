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
 *    Volodymyr Kuznetsov <vova.kuznetsov@epfl.ch>
 *    Vitaly Chipounov <vitaly.chipounov@epfl.ch>
 *
 * All contributors are listed in S2E-AUTHORS file.
 *
 */

#ifndef S2E_EXECUTIONSTATE_H
#define S2E_EXECUTIONSTATE_H

#include <klee/ExecutionState.h>
#include <klee/Memory.h>

#include "S2EStatsTracker.h"
#include "MemoryCache.h"
#include "s2e_config.h"

extern "C" {
    struct TranslationBlock;
    struct TimersState;
}



// XXX
struct CPUX86State;
#define CPU_OFFSET(field) offsetof(CPUX86State, field)

#include <llvm/ADT/DenseMap.h>
#include <llvm/ADT/SmallVector.h>
#include <tr1/unordered_map>

namespace s2e {

class Plugin;
class PluginState;
class S2EDeviceState;
class S2EExecutionState;
struct S2ETranslationBlock;

//typedef std::tr1::unordered_map<const Plugin*, PluginState*> PluginStateMap;
typedef std::map<const Plugin*, PluginState*> PluginStateMap;
typedef PluginState* (*PluginStateFactory)(Plugin *p, S2EExecutionState *s);

typedef MemoryCachePool<klee::ObjectPair,
                S2E_RAM_OBJECT_BITS,
                12, //XXX: FIX THIS HARD-CODED STUFF!
                S2E_MEMCACHE_SUPERPAGE_BITS> S2EMemoryCache;

struct S2EPhysCacheEntry
{
    uint64_t hostPage;
    klee::ObjectState *os;
    uint8_t *concreteStore;

    S2EPhysCacheEntry() {
        os = NULL;
        concreteStore = NULL;
    }
};

class S2EExecutionState : public klee::ExecutionState
{
protected:
    friend class S2EExecutor;

    static unsigned s_lastSymbolicId;

    /** Unique numeric ID for the state */
    int m_stateID;

    PluginStateMap m_PluginState;

    bool m_symbexEnabled;

    /* Internal variable - set to PC where execution should be
       switched to symbolic (e.g., due to access to symbolic memory */
    uint64_t m_startSymbexAtPC;

    /** Set to true when the state is active (i.e., currently selected).
        NOTE: for active states, SharedConcrete memory objects are stored
              in shared locations, for inactive - in ObjectStates. */
    bool m_active;

    /** Set to true when the state is killed. The cpu loop actively checks
        for such a condition, and, when met, asks the scheduler to get a new
        state */
    bool m_zombie;

    /** Set to true when the state executes code in concrete mode.
        NOTE: When m_runningConcrete is true, CPU registers that contain
              concrete values are stored in the shared region (env global
              variable), all other CPU registers are stored in ObjectState.
    */
    bool m_runningConcrete;

    typedef std::set<std::pair<uint64_t,uint64_t> > ToRunSymbolically;
    ToRunSymbolically m_toRunSymbolically;


    /* Move the following to S2EExecutor? */
    /* Mostly accessed from S2EExecutionState anyway, extra indirection if moved...*/
    /* Static because they do not change */
    static klee::MemoryObject* m_cpuRegistersState;
    static klee::MemoryObject* m_cpuSystemState;

    klee::ObjectState *m_cpuRegistersObject;
    klee::ObjectState *m_cpuSystemObject;

    static klee::MemoryObject* m_dirtyMask;
    klee::ObjectState *m_dirtyMaskObject;

    S2EDeviceState *m_deviceState;

    S2EMemoryCache m_memcache;

    /* The following structure is used to store QEMU time accounting
       variables while the state is inactive */
    TimersState* m_timersState;
    int64_t m_qemuIcount;

    S2ETranslationBlock* m_lastS2ETb;

    uint64_t m_lastMergeICount;

    bool m_needFinalizeTBExec;

    unsigned m_nextSymbVarId;

    S2EStateStats m_stats;

    /**
     * The following optimizes tracks the location of every ObjectState
     * in the TLB in order to optimize TLB updates.
     */
    typedef std::pair<unsigned int, unsigned int> TlbCoordinates;
    typedef llvm::SmallVector<TlbCoordinates, 8> ObjectStateTlbReferences;
    typedef std::tr1::unordered_map<klee::ObjectState *, ObjectStateTlbReferences> TlbMap;

    TlbMap m_tlbMap;

    /** Set when execution enters doInterrupt, reset when it exits. */
    bool m_runningExceptionEmulationCode;

    ExecutionState* clone();
    void addressSpaceChange(const klee::MemoryObject *mo,
                            const klee::ObjectState *oldState,
                            klee::ObjectState *newState);

    std::string getUniqueVarName(const std::string &name);

public:
    enum AddressType {
        VirtualAddress, PhysicalAddress, HostAddress
    };

    S2EExecutionState(klee::KFunction *kf);
    ~S2EExecutionState();

    int getID() const { return m_stateID; }

    S2EDeviceState *getDeviceState() const {
        return m_deviceState;
    }

    TranslationBlock *getTb() const;

    uint64_t getTotalInstructionCount();
    /*************************************************/

    PluginState* getPluginState(Plugin *plugin, PluginStateFactory factory) {
        PluginStateMap::iterator it = m_PluginState.find(plugin);
        if (it == m_PluginState.end()) {
            PluginState *ret = factory(plugin, this);
            assert(ret);
            m_PluginState[plugin] = ret;
            return ret;
        }
        return (*it).second;
    }

    /** Returns true is this is the active state */
    bool isActive() const { return m_active; }

    bool isZombie() const { return m_zombie; }
    void zombify() { m_zombie = true; }

    /** Returns true if this state is currently running in concrete mode */
    bool isRunningConcrete() const { return m_runningConcrete; }

    /** Returns a mask of registers that contains symbolic values */
    uint64_t getSymbolicRegistersMask() const;

    /** Read CPU general purpose register */
    klee::ref<klee::Expr> readCpuRegister(unsigned offset,
                                          klee::Expr::Width width) const;

    /** Write CPU general purpose register */
    void writeCpuRegister(unsigned offset, klee::ref<klee::Expr> value);

    /** Read concrete value from general purpose CPU register */
    bool readCpuRegisterConcrete(unsigned offset, void* buf, unsigned size);

    /** Write concrete value to general purpose CPU register */
    void writeCpuRegisterConcrete(unsigned offset, const void* buf, unsigned size);

    /** Read CPU system state */
    uint64_t readCpuState(unsigned offset, unsigned width) const;

    /** Write CPU system state */
    void writeCpuState(unsigned offset, uint64_t value, unsigned width);

    uint64_t getPc() const;
    uint64_t getPid() const;
    uint64_t getSp() const;
    uint64_t getFlags();

    void setPc(uint64_t pc);
    void setSp(uint64_t sp);

    bool getReturnAddress(uint64_t *retAddr);
    bool bypassFunction(unsigned paramCount);

    void jumpToSymbolic();
    void jumpToSymbolicCpp();
    bool needToJumpToSymbolic() const;
    void undoCallAndJumpToSymbolic();

    void dumpStack(unsigned count);
    void dumpStack(unsigned count, uint64_t sp);

    bool isForkingEnabled() const { return !forkDisabled; }
    void setForking(bool enable) {
        forkDisabled = !enable;
    }

    void enableForking();
    void disableForking();


    bool isSymbolicExecutionEnabled() const {
        return m_symbexEnabled;
    }

    bool isRunningExceptionEmulationCode() const {
        return m_runningExceptionEmulationCode;
    }

    inline void setRunningExceptionEmulationCode(bool val) {
        m_runningExceptionEmulationCode = val;
    }

    void enableSymbolicExecution();
    void disableSymbolicExecution();

    /** Return true if hostAddr is registered as a RAM with KLEE */
    bool isRamRegistered(uint64_t hostAddress);

    /** Return true if hostAddr is registered as a RAM with KLEE */
    bool isRamSharedConcrete(uint64_t hostAddress);


    /** Read value from memory, returning false if the value is symbolic */
    bool readMemoryConcrete(uint64_t address, void *buf, uint64_t size,
                            AddressType addressType = VirtualAddress);

    /** Write concrete value to memory */
    bool writeMemoryConcrete(uint64_t address, void *buf,
                             uint64_t size, AddressType addressType=VirtualAddress);



    /** Read from physical memory, switching to symbex if
        the memory contains symbolic value. Note: this
        function will use longjmp to qemu cpu loop */
    void readRamConcreteCheck(uint64_t hostAddress, uint8_t* buf, uint64_t size);


    /** Read from physical memory, concretizing if nessecary.
        Note: this function accepts host address (as returned
        by qemu_get_ram_ptr */
    void readRamConcrete(uint64_t hostAddress, uint8_t* buf, uint64_t size);

    /** Write concrete data to RAM. Optimized for host addresses */
    void writeRamConcrete(uint64_t hostAddress, const uint8_t* buf, uint64_t size);

    /** Read from CPU state. Concretize if necessary */
    void readRegisterConcrete(
            CPUX86State *cpuState, unsigned offset, uint8_t* buf, unsigned size);

    /** Write concrete data to the CPU */
    /** XXX: do we really also need writeCpuRegisterConcrete? **/
    void writeRegisterConcrete(CPUX86State *cpuState,
                               unsigned offset, const uint8_t* buf, unsigned size);

    /** Read an ASCIIZ string from memory */
    bool readString(uint64_t address, std::string &s, unsigned maxLen=256);
    bool readUnicodeString(uint64_t address, std::string &s, unsigned maxLen=256);

    /** Virtual address translation (debug mode). Returns -1 on failure. */
    uint64_t getPhysicalAddress(uint64_t virtualAddress) const;

    /** Address translation (debug mode). Returns host address or -1 on failure */
    uint64_t getHostAddress(uint64_t address,
                            AddressType addressType = VirtualAddress) const;

    /** Access to state's memory. Address is virtual or physical,
        depending on 'physical' argument. Returns NULL or false in
        case of failure (can't resolve virtual address or physical
        address is invalid) */
    klee::ref<klee::Expr> readMemory(uint64_t address,
                             klee::Expr::Width width,
                             AddressType addressType = VirtualAddress) const;
    klee::ref<klee::Expr> readMemory8(uint64_t address,
                              AddressType addressType = VirtualAddress) const;

    bool readMemoryConcrete8(uint64_t address,
                             uint8_t *result = NULL,
                             AddressType addressType = VirtualAddress,
                             bool addConstraint = true);

    bool writeMemory(uint64_t address,
                     klee::ref<klee::Expr> value,
                     AddressType addressType = VirtualAddress);
    bool writeMemory(uint64_t address,
                     uint8_t* buf,
                     klee::Expr::Width width,
                     AddressType addressType = VirtualAddress);

    bool writeMemory8(uint64_t address,
                      klee::ref<klee::Expr> value,
                      AddressType addressType = VirtualAddress);
    bool writeMemory8 (uint64_t address, uint8_t  value,
                       AddressType addressType = VirtualAddress);
    bool writeMemory16(uint64_t address, uint16_t value,
                       AddressType addressType = VirtualAddress);
    bool writeMemory32(uint64_t address, uint32_t value,
                       AddressType addressType = VirtualAddress);
    bool writeMemory64(uint64_t address, uint64_t value,
                       AddressType addressType = VirtualAddress);

    /** Fast routines used by the DMA subsystem */
    void dmaRead(uint64_t hostAddress, uint8_t *buf, unsigned size);
    void dmaWrite(uint64_t hostAddress, uint8_t *buf, unsigned size);

    /** Dirty mask management */
    uint8_t readDirtyMask(uint64_t host_address);
    void writeDirtyMask(uint64_t host_address, uint8_t val);
    void registerDirtyMask(uint64_t host_address, uint64_t size);


    CPUX86State *getConcreteCpuState() const;

    virtual void addConstraint(klee::ref<klee::Expr> e);

    /** Creates new unconstrained symbolic value */
    klee::ref<klee::Expr> createSymbolicValue(
                const std::string& name = std::string(), klee::Expr::Width width = klee::Expr::Int32);

    std::vector<klee::ref<klee::Expr> > createSymbolicArray(
                const std::string& name = std::string(), unsigned size = 4);

    /** Create a symbolic value tied to an example concrete value */
    /** If the concrete buffer is empty, creates a purely symbolic value */
    klee::ref<klee::Expr> createConcolicValue(
            const std::string& name,
            klee::Expr::Width width,
            std::vector<unsigned char> &buffer);

    std::vector<klee::ref<klee::Expr> > createConcolicArray(
                const std::string& name,
                unsigned size,
                std::vector<unsigned char> &concreteBuffer);

    /** Debug functions **/
    void dumpX86State(llvm::raw_ostream &os) const;

    /** Attempt to merge two states */
    bool merge(const ExecutionState &b);

    void updateTlbEntry(CPUX86State* env,
                              int mmu_idx, uint64_t virtAddr, uint64_t hostAddr);
    void flushTlbCache();

    void flushTlbCachePage(klee::ObjectState *objectState, int mmu_idx, int index);
};

//Some convenience macros
#define SREAD(state, addr, val) if (!state->readMemoryConcrete(addr, &val, sizeof(val))) { return; }
#define SREADR(state, addr, val) if (!state->readMemoryConcrete(addr, &val, sizeof(val))) { return false; }

}

#endif // S2E_EXECUTIONSTATE_H
