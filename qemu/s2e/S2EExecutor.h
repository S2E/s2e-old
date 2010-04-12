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

    S2EExecutionState* m_currentState;
    klee::KFunction* m_dummyMain;

public:
    S2EExecutor(S2E* s2e, TCGLLVMContext *tcgLVMContext,
                const InterpreterOptions &opts,
                klee::InterpreterHandler *ie);
    ~S2EExecutor();

    S2EExecutionState* getCurrentState() { return m_currentState; }

    void updateCurrentState(CPUX86State* cpuState, uint64_t pc);
    uintptr_t executeTranslationBlock(
            TranslationBlock *tb, void* volatile* saved_AREGs);
};

} // namespace s2e

#endif // S2E_EXECUTOR_H
