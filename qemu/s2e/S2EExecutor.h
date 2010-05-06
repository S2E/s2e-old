#ifndef S2E_EXECUTOR_H
#define S2E_EXECUTOR_H

#include <klee/Executor.h>

class TCGLLVMContext;

struct TranslationBlock;
struct CPUX86State;

namespace s2e {

class S2E;
class S2EExecutionState;

/** Handler required for KLEE interpreter */
class S2EHandler : public klee::InterpreterHandler
{
private:
    S2E* m_s2e;
    unsigned m_testIndex;  // number of tests written so far
    unsigned m_pathsExplored; // number of paths explored so far

public:
    S2EHandler(S2E* s2e);

    std::ostream &getInfoStream() const;
    std::string getOutputFilename(const std::string &fileName);
    std::ostream *openOutputFile(const std::string &fileName);

    /* klee-related function */
    void incPathsExplored();

    /* klee-related function */
    void processTestCase(const klee::ExecutionState &state,
                         const char *err, const char *suffix);
};


class S2EExecutor : public klee::Executor
{
protected:
    S2E* m_s2e;
    TCGLLVMContext* m_tcgLLVMContext;

    klee::KFunction* m_dummyMain;

    /* Unused memory regions that should be unmapped.
       Copy-then-unmap is used in order to catch possible
       direct memory accesses from QEMU code. */
    std::vector< std::pair<uint64_t, uint64_t> > m_unusedMemoryRegions;

    std::vector<klee::MemoryObject*> m_saveOnContextSwitch;

    static void handlerTraceMemoryAccess(klee::Executor* executor,
                                    klee::ExecutionState* state,
                                    klee::KInstruction* target,
                                    std::vector<klee::ref<klee::Expr> > &args);

public:
    S2EExecutor(S2E* s2e, TCGLLVMContext *tcgLVMContext,
                const InterpreterOptions &opts,
                klee::InterpreterHandler *ie);
    ~S2EExecutor();

    /** Create initial execution state */
    S2EExecutionState* createInitialState();

    /** Called from QEMU before entering main loop */
    void initializeExecution(S2EExecutionState *initialState);

    void registerCpu(S2EExecutionState *initialState, CPUX86State *cpuEnv);
    void registerRam(S2EExecutionState *initialState,
                        uint64_t startAddress, uint64_t size,
                        uint64_t hostAddress, bool isSharedConcrete,
                        bool saveOnContextSwitch=true);

    /** Return true if hostAddr is registered as a RAM with KLEE */
    bool isRamRegistered(S2EExecutionState *state, uint64_t hostAddress);

    /** Return true if hostAddr is registered as a RAM with KLEE */
    bool isRamSharedConcrete(S2EExecutionState *state, uint64_t hostAddress);

    /** Read from physical memory, switching to symbex if
        the memory contains symbolic value. Note: this
        function will use longjmp to qemu cpu loop */
    void readRamConcreteCheck(S2EExecutionState *state,
            uint64_t hostAddress, uint8_t* buf, uint64_t size);

    /** Read from physical memory, concretizing if nessecary.
        Note: this function accepts host address (as returned
        by qemu_get_ram_ptr */
    void readRamConcrete(S2EExecutionState *state,
            uint64_t hostAddress, uint8_t* buf, uint64_t size);

    /** Write concrete value to physical memory.
        Note: this function accepts host address (as returned
        by qemu_get_ram_ptr */
    void writeRamConcrete(S2EExecutionState *state,
            uint64_t hostAddress, const uint8_t* buf, uint64_t size);

    S2EExecutionState* selectNextState(S2EExecutionState* state);

    uintptr_t executeTranslationBlock(S2EExecutionState *state,
                                      TranslationBlock *tb);

    void cleanupTranslationBlock(S2EExecutionState *state,
                                 TranslationBlock *tb);

    /** Enable symbolic execution for a given state */
    void enableSymbolicExecution(S2EExecutionState* state);

    /** Disable symbolic execution for a given state */
    void disableSymbolicExecution(S2EExecutionState* state);

protected:
    void doStateSwitch(S2EExecutionState* oldState,
                       S2EExecutionState* newState);

    void doStateFork(S2EExecutionState *originalState,
                     const std::vector<S2EExecutionState*>& newStates,
                     const std::vector<klee::ref<klee::Expr> >& conditions);

    /** Copy concrete values to their proper location, concretizing
        if necessary (will not touch RAM - it is always symbolic) */
    void copyOutConcretes(klee::ExecutionState &state);

    /** Copy values from their proper location to the state */
    bool copyInConcretes(klee::ExecutionState &state);

    /** Called on fork, used to trace forks */
    StatePair fork(klee::ExecutionState &current,
                   klee::ref<klee::Expr> condition, bool isInternal);

    /** Called on branches, used to trace forks */
    void branch(klee::ExecutionState &state,
              const std::vector< klee::ref<klee::Expr> > &conditions,
              std::vector<klee::ExecutionState*> &result);
};

} // namespace s2e

#endif // S2E_EXECUTOR_H
