#ifndef S2E_EXECUTIONSTATE_H
#define S2E_EXECUTIONSTATE_H

#include <klee/ExecutionState.h>
#include <klee/Memory.h>

extern "C" {
    struct TranslationBlock;
}

// XXX
struct CPUX86State;
#define CPU_OFFSET(field) offsetof(CPUX86State, field)

//#include <tr1/unordered_map>

namespace s2e {

class Plugin;
class PluginState;
class S2EDeviceState;

//typedef std::tr1::unordered_map<const Plugin*, PluginState*> PluginStateMap;
typedef std::map<const Plugin*, PluginState*> PluginStateMap;
typedef PluginState* (*PluginStateFactory)(Plugin *p, S2EExecutionState *s);

class S2ECPUObjectCache {
private:
    const klee::MemoryObject *m_mo;
    const klee::ObjectState *m_os;
public:
    S2ECPUObjectCache() {
        m_mo = NULL;
        m_os = NULL;
    }

    inline const klee::ObjectState *lookup(const klee::MemoryObject *mo) const {
        if (mo == m_mo) {
            return m_os;
        }
        return NULL;
    }

    inline void update(const klee::MemoryObject *mo, const klee::ObjectState *os) {
        m_os = os;
        m_mo = mo;
    }

};

template <unsigned Size=101>
class S2EMemObjectCache {
private:
    struct CacheEntry {
        uint64_t address;
        klee::ObjectPair objPair;
    };

    CacheEntry m_entries[Size];
    mutable uint64_t m_hits, m_misses;
    mutable uint64_t m_cval, m_chash;

    inline uint64_t hash(uint64_t Val) const {
        /*if (Val == m_cval) {
            return m_chash;
        }

        m_cval = Val;
        m_chash = Val % Size;
        return m_chash;*/
        return Val % Size;

    }
public:
    S2EMemObjectCache() {
        for (unsigned i=0; i<Size; ++i) {
            m_entries[i].address = (uint64_t)-1;
            m_entries[i].objPair = klee::ObjectPair(NULL, NULL);
        }
        m_hits = 0;
        m_misses = 0;
        m_cval = 0;
        m_chash = 0;
    }

    inline klee::ObjectPair lookup(uint64_t address) const{

        unsigned ha = hash(address);
        if (address == m_entries[ha].address) {
            return m_entries[ha].objPair;
        }
        return klee::ObjectPair(NULL, NULL);
    }

    inline void update(uint64_t address, const klee::ObjectPair &p) {
        unsigned ha = hash(address);
        m_entries[ha].address = address;
        m_entries[ha].objPair = p;
        assert(p.second && p.first);
    }


};

/** Dummy implementation, just to make events work */
class S2EExecutionState : public klee::ExecutionState
{
protected:
    friend class S2EExecutor;

    static int s_lastStateID;
    int m_stateID;

    PluginStateMap m_PluginState;

    bool m_symbexEnabled;
    uint64_t m_startSymbexAtPC;

    /* Move the following to S2EExecutor */
    bool m_active;
    bool m_runningConcrete;

    /* Move the following to S2EExecutor */
    klee::MemoryObject* m_cpuRegistersState;
    klee::MemoryObject* m_cpuSystemState;

    /* Object caching scheme */
    //cpu cache will not work because KLEE may internally do copy on write
    //mutable S2ECPUObjectCache m_cpuCache;
    mutable S2EMemObjectCache<101> m_memCache;

    klee::ObjectState *m_cpuRegistersObject;
    klee::ObjectState *m_cpuSystemObject;

    ExecutionState* clone();

    S2EDeviceState *m_deviceState;

public:
    S2EExecutionState(klee::KFunction *kf);

    int getID() const { return m_stateID; }

    S2EDeviceState *getDeviceState() const {
        return m_deviceState;
    }

    TranslationBlock *getTb() const;

    /*************************************************/
#if 0
    /** Accesses to register objects through the cache **/
    inline const klee::ObjectState* fetchObjectStateReg(const klee::MemoryObject *mo) const {
        const klee::ObjectState* os;
        if (!(os = m_cpuCache.lookup(mo))) {
            os = addressSpace.findObject(mo);
            m_cpuCache.update(mo, os);
        }
        return os;
    }

    inline klee::ObjectState* fetchObjectStateRegWritable(const klee::MemoryObject *mo, const klee::ObjectState *os) {
        klee::ObjectState *wos = addressSpace.getWriteable(mo, os);
        m_cpuCache.update(mo, wos);
        return wos;
    }
#endif

    /** Accesses to memory objects through the cache **/
    inline klee::ObjectPair fetchObjectStateMem(uint64_t hostAddress, uint64_t tpm) const {
        klee::ObjectPair op;
        if ((op = m_memCache.lookup(hostAddress  & tpm)).first == NULL)
        {
            op = addressSpace.findObject(hostAddress);
            m_memCache.update(hostAddress & tpm, op);
        }
        return op;
    }

    inline klee::ObjectState* fetchObjectStateMemWritable(const klee::MemoryObject *mo, const klee::ObjectState *os) {
        klee::ObjectState *wos = addressSpace.getWriteable(mo, os);
        m_memCache.update(mo->address, klee::ObjectPair(mo,wos));
        return wos;
    }

    /** Universal access **/
    inline const klee::ObjectState* fetchObjectState(const klee::MemoryObject *mo, uint64_t tpm) const {
        if (mo == m_cpuRegistersState || mo == m_cpuSystemState) {
            return addressSpace.findObject(mo);
        }else {
            return fetchObjectStateMem(mo->address, tpm).second;
        }
    }

    inline klee::ObjectState* fetchObjectStateWritable(const klee::MemoryObject *mo, const klee::ObjectState *os) {
        if (mo == m_cpuRegistersState || mo == m_cpuSystemState) {
            //return fetchObjectStateRegWritable(mo, os);
            return addressSpace.getWriteable(mo, os);
        }else {
            return fetchObjectStateMemWritable(mo, os);
        }
    }

    /*************************************************/

    PluginState* getPluginState(Plugin *plugin, PluginStateFactory factory) {
        PluginStateMap::iterator it = m_PluginState.find(plugin);
        if (it == m_PluginState.end()) {
            PluginState *ret = factory(plugin, this);
            m_PluginState[plugin] = ret;
            return ret;
        }
        return (*it).second;
    }

    /** Returns true is this is the active state */
    bool isActive() const { return m_active; }

    /** Returns true if this state is currently running in concrete mode.
        That means that either current TB is executed entirely concrete,
        or that symbolically running TB code have called concrete helper */
    bool isRunningConcrete() const { return m_runningConcrete; }

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

    /** Returns true if symbex is currently enabled for this state */
    bool isSymbolicExecutionEnabled() const { return m_symbexEnabled; }

    /** Read value from memory, returning false if the value is symbolic */
    bool readMemoryConcrete(uint64_t address, void *dest, uint64_t size);

    /** Write concrete value to memory */
    void writeMemoryConcrete(uint64_t address, uint64_t value, uint64_t size);

    /** Read an ASCIIZ string from memory */
    bool readString(uint64_t address, std::string &s, unsigned maxLen=256);
    bool readUnicodeString(uint64_t address, std::string &s, unsigned maxLen=256);

    /** Virtual address translation (debug mode). Return -1 on failure. */
    uint64_t getPhysicalAddress(uint64_t virtualAddress) const;

    /** Access to state's memory. Address is virtual or physical,
        depending on 'physical' argument. Returns NULL or false in
        case of failure (can't resolve virtual address or physical
        address is invalid) */
    klee::ref<klee::Expr> readMemory(uint64_t address,
                                     klee::Expr::Width width,
                                     bool physical = false) const;
    klee::ref<klee::Expr> readMemory8(uint64_t address,
                                      bool physical = false) const;

    bool writeMemory(uint64_t address,
                     klee::ref<klee::Expr> value,
                     bool physical = false);
    bool writeMemory(uint64_t address,
                     uint8_t* buf,
                     klee::Expr::Width width,
                     bool physical = false);

    bool writeMemory8(uint64_t address,
                      klee::ref<klee::Expr> value, bool physical = false);
    bool writeMemory8 (uint64_t address, uint8_t  value, bool physical = false);
    bool writeMemory16(uint64_t address, uint16_t value, bool physical = false);
    bool writeMemory32(uint64_t address, uint32_t value, bool physical = false);
    bool writeMemory64(uint64_t address, uint64_t value, bool physical = false);

    /** Creates new unconstrained symbolic value */
    klee::ref<klee::Expr> createSymbolicValue(klee::Expr::Width width,
                              const std::string& name = std::string());

    std::vector<klee::ref<klee::Expr> > createSymbolicArray(
            unsigned size, const std::string& name = std::string());
};

//Some convenience macros
#define SREAD(state, addr, val) if (!state->readMemoryConcrete(addr, &val, sizeof(val))) { return; }
#define SREADR(state, addr, val) if (!state->readMemoryConcrete(addr, &val, sizeof(val))) { return false; }

}

#endif // S2E_EXECUTIONSTATE_H
