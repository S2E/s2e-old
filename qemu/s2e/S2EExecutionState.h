#ifndef S2E_EXECUTIONSTATE_H
#define S2E_EXECUTIONSTATE_H

#include <klee/ExecutionState.h>

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

    ExecutionState* clone();

    S2EDeviceState *m_deviceState;

public:
    S2EExecutionState(klee::KFunction *kf);

    int getID() const { return m_stateID; }

    S2EDeviceState *getDeviceState() const {
        return m_deviceState;
    }

    TranslationBlock *getTb() const;

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
    static klee::ref<klee::Expr> createSymbolicValue(klee::Expr::Width width,
                              const std::string& name = std::string());

    static std::vector<klee::ref<klee::Expr> > createSymbolicArray(
            unsigned size, const std::string& name = std::string());
};

//Some convenience macros
#define SREAD(state, addr, val) if (!state->readMemoryConcrete(addr, &val, sizeof(val))) { return; }
#define SREADR(state, addr, val) if (!state->readMemoryConcrete(addr, &val, sizeof(val))) { return false; }

}

#endif // S2E_EXECUTIONSTATE_H
