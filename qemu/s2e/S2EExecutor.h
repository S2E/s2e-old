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
 * All contributors are listed in the S2E-AUTHORS file.
 */

#ifndef S2E_EXECUTOR_H
#define S2E_EXECUTOR_H

#include <klee/Executor.h>
#include <llvm/Support/raw_ostream.h>
#include <cpu.h>

class TCGLLVMContext;

struct TranslationBlock;
/* struct */ CPUArchState;

namespace klee {
    class Query;
}

namespace s2e {

class S2E;
class S2EExecutionState;
struct S2ETranslationBlock;

class CpuExitException
{
};

/** Handler required for KLEE interpreter */
class S2EHandler : public klee::InterpreterHandler
{
private:
    S2E* m_s2e;
    // /* UNUSED */ unsigned m_testIndex;  // number of tests written so far
    unsigned m_pathsExplored; // number of paths explored so far

public:
    S2EHandler(S2E* s2e);

    llvm::raw_ostream &getInfoStream() const;
    std::string getOutputFilename(const std::string &fileName);
    llvm::raw_ostream *openOutputFile(const std::string &fileName);

    /* klee-related function */
    void incPathsExplored();

    /* klee-related function */
    void processTestCase(const klee::ExecutionState &state,
                         const char *err, const char *suffix);
};


typedef void (*StateManagerCb)(S2EExecutionState *s, bool killingState);

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

    std::vector<S2EExecutionState*> m_deletedStates;

    bool m_executeAlwaysKlee;

    bool m_forceConcretizations;

    bool m_forkProcTerminateCurrentState;

    bool m_inLoadBalancing;

    struct QEMUTimer *m_stateSwitchTimer;

    /** Holds the yielded state, if any */
    S2EExecutionState* yieldedState;

    /** Moves yielded state back into list of schedulable states */
    void restoreYieldedState(void);

public:
    S2EExecutor(S2E* s2e, TCGLLVMContext *tcgLVMContext,
                const InterpreterOptions &opts,
                klee::InterpreterHandler *ie);
    virtual ~S2EExecutor();

    void flushTb();

    /** Create initial execution state */
    S2EExecutionState* createInitialState();

    /** Called from QEMU before entering main loop */
    void initializeExecution(S2EExecutionState *initialState,
                             bool executeAlwaysKlee);

#ifdef TARGET_ARM
    void registerCpu(S2EExecutionState *initialState, CPUARMState *cpuEnv);
#elif defined(TARGET_I386)
    void registerCpu(S2EExecutionState *initialState, CPUX86State *cpuEnv);
#endif



    void registerRam(S2EExecutionState *initialState,
                        uint64_t startAddress, uint64_t size,
                        uint64_t hostAddress, bool isSharedConcrete,
                        bool saveOnContextSwitch=true, const char *name="");

    void registerDirtyMask(S2EExecutionState *initial_state,
                           uint64_t host_address, uint64_t size);

    /* Execute llvm function in current context */
    klee::ref<klee::Expr> executeFunction(S2EExecutionState *state,
                            llvm::Function *function,
                            const std::vector<klee::ref<klee::Expr> >& args
                                = std::vector<klee::ref<klee::Expr> >());

    klee::ref<klee::Expr> executeFunction(S2EExecutionState *state,
                            const std::string& functionName,
                            const std::vector<klee::ref<klee::Expr> >& args
                                = std::vector<klee::ref<klee::Expr> >());

    /* Functions to be called mainly from QEMU */

    S2EExecutionState* selectNextState(S2EExecutionState* state);
    klee::ExecutionState* selectNonSpeculativeState(S2EExecutionState *state);

    uintptr_t executeTranslationBlock(S2EExecutionState *state,
                                      TranslationBlock *tb);

    /* Returns true if the CPU loop must be exited */
    bool finalizeTranslationBlockExec(S2EExecutionState *state);

    void cleanupTranslationBlock(S2EExecutionState *state);

    void updateStates(klee::ExecutionState *current) {
        klee::Executor::updateStates(current);
    }


    void setCCOpEflags(S2EExecutionState *state);
#ifdef TARGET_ARM
    void doInterrupt(S2EExecutionState *state);
#elif defined(TARGET_I386)
    void doInterrupt(S2EExecutionState *state, int intno,
                                         int is_int, int error_code,
                                         uint64_t next_eip, int is_hw);
#endif

    /** Suspend the given state (does not kill it) */
    bool suspendState(S2EExecutionState *state, bool onlyRemoveFromPtree = false);

    /** Puts back the previously suspended state in the queue */
    bool resumeState(S2EExecutionState *state, bool onlyAddToPtree = false);

    klee::Searcher *getSearcher() const {
        return searcher;
    }

    void setSearcher(klee::Searcher *s) {
        searcher = s;
    }

    /** Called on fork, used to trace forks */
    StatePair fork(klee::ExecutionState &current,
                   klee::ref<klee::Expr> condition, bool isInternal);

    bool merge(klee::ExecutionState &base, klee::ExecutionState &other);

    void setForceConcretizations(bool b) {
        m_forceConcretizations = true;
    }

    void unrefS2ETb(S2ETranslationBlock* s2e_tb);

    void queueStateForMerge(S2EExecutionState *state);

    void initializeStatistics();

    void updateStats(S2EExecutionState *state);

    bool isLoadBalancing() const {
        return m_inLoadBalancing;
    }

    /** Kill the state with test case generation */
    virtual void terminateStateEarly(klee::ExecutionState &state, const llvm::Twine &message);

    /** Yields the specified state and raises an exception to exit the cpu loop */
    virtual void yieldState(klee::ExecutionState &state);
    const S2EExecutionState* getYieldedState() {
        return yieldedState;
    }

protected:
    static void handlerTraceMemoryAccess(klee::Executor* executor,
                                    klee::ExecutionState* state,
                                    klee::KInstruction* target,
                                    std::vector<klee::ref<klee::Expr> > &args);

    //Traces every single LLVM instruction in dyngend code
    static void handlerTraceInstruction(klee::Executor* executor,
                                    klee::ExecutionState* state,
                                    klee::KInstruction* target,
                                    std::vector<klee::ref<klee::Expr> > &args);

    static void handlerTracePortAccess(klee::Executor* executor,
                                         klee::ExecutionState* state,
                                         klee::KInstruction* target,
                                         std::vector<klee::ref<klee::Expr> > &args);

    static void handlerOnTlbMiss(klee::Executor* executor,
                                         klee::ExecutionState* state,
                                         klee::KInstruction* target,
                                         std::vector<klee::ref<klee::Expr> > &args);

    static void handleForkAndConcretize(klee::Executor* executor,
                                         klee::ExecutionState* state,
                                         klee::KInstruction* target,
                                         std::vector<klee::ref<klee::Expr> > &args);

    static void handleMakeSymbolic(klee::Executor* executor,
                                   klee::ExecutionState* state,
                                   klee::KInstruction* target,
                                   std::vector<klee::ref<klee::Expr> > &args);

    static void handleGetValue(klee::Executor* executor,
                               klee::ExecutionState* state,
                               klee::KInstruction* target,
                               std::vector<klee::ref<klee::Expr> > &args);
    
    void prepareFunctionExecution(S2EExecutionState *state,
                           llvm::Function* function,
                           const std::vector<klee::ref<klee::Expr> >& args);
    bool executeInstructions(S2EExecutionState *state, unsigned callerStackSize = 1);

    uintptr_t executeTranslationBlockKlee(S2EExecutionState *state,
                                          TranslationBlock *tb);

    uintptr_t executeTranslationBlockConcrete(S2EExecutionState *state,
                                              TranslationBlock *tb);

    void deleteState(klee::ExecutionState *state);

    void doStateSwitch(S2EExecutionState* oldState,
                       S2EExecutionState* newState);

    void doStateFork(S2EExecutionState *originalState,
                     const std::vector<S2EExecutionState*>& newStates,
                     const std::vector<klee::ref<klee::Expr> >& conditions);

    void doLoadBalancing();

    /** Copy concrete values to their proper location, concretizing
        if necessary (most importantly it will concretize CPU registers.
        Note: this is required only to execute generated code,
        other QEMU components access all registers through wrappers. */
    void switchToConcrete(S2EExecutionState *state);

    /** Copy concrete values to the execution state storage */
    void switchToSymbolic(S2EExecutionState *state);


    /** Implementation that does nothing. We do not need to concretize
        when calling externals, because all of them access data only
        through wrappers. */
    void copyOutConcretes(klee::ExecutionState &state);

    /** Implementation that does nothing. We do not need to concretize
        when calling externals, because all of them access data only
        through wrappers. */
    bool copyInConcretes(klee::ExecutionState &state);



    /** Called on branches, used to trace forks */
    void branch(klee::ExecutionState &state,
              const std::vector< klee::ref<klee::Expr> > &conditions,
              std::vector<klee::ExecutionState*> &result);

    void notifyBranch(klee::ExecutionState &state);

    /** Kills the specified state and raises an exception to exit the cpu loop */
    virtual void terminateState(klee::ExecutionState &state);

    /** Kills the specified state without exiting to the CPU loop */
    void terminateStateAtFork(S2EExecutionState &state);

    void setupTimersHandler();
    void initializeStateSwitchTimer();
    static void stateSwitchTimerCallback(void *opaque);

    /** The following are special handlers for MMU functions */
    static void handle_ldb_mmu(klee::Executor* executor,
                        klee::ExecutionState* state,
                        klee::KInstruction* target,
                        std::vector< klee::ref<klee::Expr> > &args);

    static void handle_ldw_mmu(klee::Executor* executor,
                        klee::ExecutionState* state,
                        klee::KInstruction* target,
                        std::vector< klee::ref<klee::Expr> > &args);

    static void handle_ldl_mmu(klee::Executor* executor,
                        klee::ExecutionState* state,
                        klee::KInstruction* target,
                        std::vector< klee::ref<klee::Expr> > &args);

    static void handle_ldq_mmu(klee::Executor* executor,
                        klee::ExecutionState* state,
                        klee::KInstruction* target,
                        std::vector< klee::ref<klee::Expr> > &args);

    static void handle_stb_mmu(klee::Executor* executor,
                        klee::ExecutionState* state,
                        klee::KInstruction* target,
                        std::vector< klee::ref<klee::Expr> > &args);

    static void handle_stw_mmu(klee::Executor* executor,
                        klee::ExecutionState* state,
                        klee::KInstruction* target,
                        std::vector< klee::ref<klee::Expr> > &args);

    static void handle_stl_mmu(klee::Executor* executor,
                        klee::ExecutionState* state,
                        klee::KInstruction* target,
                        std::vector< klee::ref<klee::Expr> > &args);

    static void handle_stq_mmu(klee::Executor* executor,
                        klee::ExecutionState* state,
                        klee::KInstruction* target,
                        std::vector< klee::ref<klee::Expr> > &args);

    static klee::ref<klee::Expr> handle_ldst_mmu(klee::Executor* executor,
                        klee::ExecutionState* state,
                        klee::KInstruction* target,
                        std::vector< klee::ref<klee::Expr> > &args,
                        bool isWrite, unsigned data_size, bool signExtend, bool zeroExtend);

    static void handle_lduw_kernel(klee::Executor* executor,
                        klee::ExecutionState* state,
                        klee::KInstruction* target,
                        std::vector< klee::ref<klee::Expr> > &args);

    static void handle_ldl_kernel(klee::Executor* executor,
                        klee::ExecutionState* state,
                        klee::KInstruction* target,
                        std::vector< klee::ref<klee::Expr> > &args);

    static void handle_ldq_kernel(klee::Executor* executor,
                        klee::ExecutionState* state,
                        klee::KInstruction* target,
                        std::vector< klee::ref<klee::Expr> > &args);

    static void handle_stl_kernel(klee::Executor* executor,
                        klee::ExecutionState* state,
                        klee::KInstruction* target,
                        std::vector< klee::ref<klee::Expr> > &args);

    static void handle_stq_kernel(klee::Executor* executor,
                        klee::ExecutionState* state,
                        klee::KInstruction* target,
                        std::vector< klee::ref<klee::Expr> > &args);

    static void handle_ldst_kernel(klee::Executor* executor,
                        klee::ExecutionState* state,
                        klee::KInstruction* target,
                        std::vector< klee::ref<klee::Expr> > &args,
                        bool isWrite, unsigned data_size, bool signExtend, bool zeroExtend);

    static klee::ref<klee::ConstantExpr> handleForkAndConcretizeNative(klee::Executor* executor,
                                               klee::ExecutionState* state,
                                               klee::KInstruction* target,
                                               std::vector< klee::ref<klee::Expr> > &args);

    void replaceExternalFunctionsWithSpecialHandlers();
    void disableConcreteLLVMHelpers();

    struct HandlerInfo {
      const char *name;
      S2EExecutor::FunctionHandler handler;
    };

    static HandlerInfo s_handlerInfo[];
};

struct S2ETranslationBlock
{
    /** Reference counter. S2ETranslationBlock should not be freed
        until all LLVM functions are completely executed. This reference
        counter controls it. */
    unsigned refCount;

    /** A copy of TranslationBlock::llvm_function that can be used
        even after TranslationBlock is destroyed */
    llvm::Function* llvm_function;

    /** A list of all instruction execution signals associated with
        this basic block. All signals in the list will be deleted
        when this translation block will be flushed.
        XXX: how could we avoid using void* here ? */
    std::vector<void*> executionSignals;
};

} // namespace s2e

#endif // S2E_EXECUTOR_H
