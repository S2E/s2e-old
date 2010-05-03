#ifndef S2E_EXECUTIONSTATE_H
#define S2E_EXECUTIONSTATE_H

#include <klee/ExecutionState.h>

// XXX
struct CPUX86State;

//#include <tr1/unordered_map>

namespace s2e {

class Plugin;
class PluginState;
class S2EDeviceState;

//typedef std::tr1::unordered_map<const Plugin*, PluginState*> PluginStateMap;
typedef std::map<const Plugin*, PluginState*> PluginStateMap;
typedef PluginState* (*PluginStateFactory)();

/** Dummy implementation, just to make events work */
class S2EExecutionState : public klee::ExecutionState
{
protected:
    friend class S2EExecutor;

    PluginStateMap m_PluginState;
    bool m_symbexEnabled;

    ExecutionState* clone();

    S2EDeviceState *m_deviceState;
public:
    CPUX86State* cpuState;
    

public:
    S2EExecutionState(klee::KFunction *kf);


    void selectState(CPUX86State* cpuState, klee::KFunction *kf);
    CPUX86State* getCpuState() const { return cpuState; }
    
    S2EDeviceState *getDeviceState() const {
        return m_deviceState;
    }

    TranslationBlock *getTb() const;

    PluginState* getPluginState(const Plugin *plugin, PluginStateFactory factory) {
        PluginStateMap::iterator it = m_PluginState.find(plugin);
        if (it == m_PluginState.end()) {
            PluginState *ret = factory();
            m_PluginState[plugin] = ret;
            return ret;
        }
        return (*it).second;
    }

    /** Read value from memory concretizing it if necessary */
    uint64_t readMemoryConcrete(uint64_t address, char size);
    bool readMemoryConcrete(uint64_t address, void *dest, char size);

    /** Write concrete value to memory */
    void writeMemoryConcrete(uint64_t address, uint64_t value, char size);

    /** Read an ASCIIZ string from memory */
    bool readString(uint64_t address, std::string &s, unsigned maxLen=256);
    bool readUnicodeString(uint64_t address, std::string &s, unsigned maxLen=256);

    uint64_t getPc() const;
    uint64_t getPid() const;
    uint64_t getSp() const;

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
                      klee::ref<klee::Expr> value, bool physical);
    bool writeMemory8 (uint64_t address, uint8_t  value, bool physical);
    bool writeMemory16(uint64_t address, uint16_t value, bool physical);
    bool writeMemory32(uint64_t address, uint32_t value, bool physical);
    bool writeMemory64(uint64_t address, uint64_t value, bool physical);

    /** Creates new unconstrained symbolic value */
    static klee::ref<klee::Expr> createSymbolicValue(klee::Expr::Width width,
                              const std::string& name = std::string());
};

//Some convenience macros
#define SREAD(state, addr, val) if (!state->readMemoryConcrete(addr, &val, sizeof(val))) { return; }
#define SREADR(state, addr, val) if (!state->readMemoryConcrete(addr, &val, sizeof(val))) { return false; }

}

#endif // S2E_EXECUTIONSTATE_H
