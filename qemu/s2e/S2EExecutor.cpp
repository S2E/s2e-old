extern "C" {
#include <qemu-common.h>
#include <cpu-all.h>
#include <tcg-llvm.h>
}

#include "S2EExecutor.h"
#include <s2e/S2E.h>
#include <s2e/S2EExecutionState.h>

#include <llvm/Module.h>
#include <llvm/Function.h>
#include <llvm/DerivedTypes.h>
#include <llvm/Instructions.h>

#include <vector>

using namespace std;
using namespace llvm;
using namespace klee;

namespace s2e {

S2EHandler::S2EHandler(S2E* s2e)
        : m_s2e(s2e)
{
}

std::ostream &S2EHandler::getInfoStream() const
{
    return m_s2e->getInfoStream();
}

std::string S2EHandler::getOutputFilename(const std::string &fileName)
{
    return m_s2e->getOutputFilename(fileName);
}

std::ostream *S2EHandler::openOutputFile(const std::string &fileName)
{
    return m_s2e->openOutputFile(fileName);
}

/* klee-related function */
void S2EHandler::incPathsExplored()
{
    m_pathsExplored++;
}

/* klee-related function */
void S2EHandler::processTestCase(const klee::ExecutionState &state,
                     const char *err, const char *suffix)
{
    m_s2e->getWarningsStream() << "Terminating state '" << (&state)
           << "with error message '" << (err ? err : "") << "'" << std::endl;
}

void S2EExecutor::callExternalFunction(ExecutionState &state,
                            KInstruction *target,
                            llvm::Function *function,
                            std::vector< ref<Expr> > &arguments)
{
    assert(0);
}

void S2EExecutor::runFunctionAsMain(llvm::Function *f,
                                 int argc,
                                 char **argv,
                                 char **envp)
{
    assert(0);
}

S2EExecutor::S2EExecutor(S2E* s2e, TCGLLVMContext *tcgLLVMContext,
                    const InterpreterOptions &opts,
                            InterpreterHandler *ie)
        : Executor(opts, ie),
          m_s2e(s2e), m_tcgLLVMContext(tcgLLVMContext)
{
    /* Add dummy main function for a module */
    const Type* voidTy = Type::getVoidTy(
            m_tcgLLVMContext->getLLVMContext());
    Function* dummyMain = Function::Create(
            FunctionType::get(voidTy, vector<const Type*>(), false),
            Function::PrivateLinkage, "s2e_dummyMainFunction",
            m_tcgLLVMContext->getModule());
    ReturnInst::Create(m_tcgLLVMContext->getLLVMContext(), BasicBlock::Create(
            m_tcgLLVMContext->getLLVMContext(), "entry", dummyMain));

    /* Set module for the executor */
    ModuleOptions MOpts(KLEE_LIBRARY_DIR,
                    /* Optimize= */ false, /* CheckDivZero= */ false);
    setModule(m_tcgLLVMContext->getModule(), MOpts);

    /* Create initial execution state */
    S2EExecutionState *state = new S2EExecutionState(
            kmodule->functionMap[dummyMain]);

    /* Make CPUState instances accessible: generated code uses them as globals */
    for(CPUState *env = first_cpu; env != NULL; env = env->next_cpu) {
        addExternalObject(*state, env, sizeof(*env), false);
    }

    states.insert(state);
    m_currentState = state;
}

/*
void S2EExecutor::updateCurrentState(
        CPUState* cpuState, uint64_t pc)
{
    assert(m_currentState);

    m_currentState->cpuState = cpuState;
    m_currectState->cpuPC = pc;

    // TODO: update KFunction and instruction iterator
}
*/

} // namespace s2e
