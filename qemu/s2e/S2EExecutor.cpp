#include "S2EExecutor.h"
#include <s2e/S2E.h>

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

S2EExecutor::S2EExecutor(const InterpreterOptions &opts, InterpreterHandler *ie)
        : Executor(opts, ie)
{
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

} // namespace s2e
