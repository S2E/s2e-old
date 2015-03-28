#include <string>
#include <s2e/S2E.h>
#include <s2e/Utils.h>
#include <s2e/S2EExecutor.h>
#include "InputGenerator.h"

namespace s2e {
namespace plugins {

S2E_DEFINE_PLUGIN(InputGenerator, "Tutorial - Generating inputs", "InputGenerator",);

void InputGenerator::initialize()
{
   s2e()->getCorePlugin()->onTestCaseGeneration.connect(
            sigc::mem_fun(*this, &InputGenerator::onInputGeneration));
}

void InputGenerator::onInputGeneration(S2EExecutionState *state, const std::string &message)
{
    pathConstraintSize = state->constraints.constraints.size();
    inputConstraintSize = state->inputConstraints.size();
    argsConstraintVectorSize = state->argsConstraintsAll.size();

    s2e()->getMessagesStream()
            << "InputGenerator: processTestCase of state " << state->getID()
            << " at address " << hexval(state->getPc())
            << '\n';

    if (argsConstraintVectorSize == 0) {
        s2e()->getWarningsStream() << "No tainted any sensitive function" << '\n';
        return;
    }

    klee::ExecutionState* exploitState =
                new klee::ExecutionState(*state);
    pruneInputConstraints(state, exploitState);

    s2e()->getDebugStream() << "========== Exploit Constraints ==========\n";
    for (int i = 0; i < exploitState->constraints.constraints.size(); i++) {
        s2e()->getDebugStream() << exploitState->constraints.constraints[i] << '\n';
    }
    s2e()->getDebugStream() << "Exploit Constraint Size: " << exploitState->constraints.constraints.size() << "\n\n";

    s2e()->getDebugStream(state) << "========== Original Constraints ==========\n";
    for (int i = 0; i < pathConstraintSize; i++) {
        s2e()->getDebugStream(state) << state->constraints.constraints[i] << '\n';
    }
    s2e()->getDebugStream(state) << "Original Constraint Size: " << pathConstraintSize << "\n\n";

}

void InputGenerator::pruneInputConstraints(
        S2EExecutionState *state,
        klee::ExecutionState *exploitState)
{
    std::vector< klee::ref<klee::Expr> >
        pathConstraints(pathConstraintSize);
    std::vector< klee::ref<klee::Expr> >::iterator it;

    it = std::set_difference(state->constraints.begin(),
                             state->constraints.end(),
                             state->inputConstraints.begin(),
                             state->inputConstraints.end(),
                             pathConstraints.begin());

    pathConstraints.resize(it - pathConstraints.begin());

    exploitState->constraints =
        *(new klee::ConstraintManager(pathConstraints));

    s2e()->getMessagesStream(state) << "Pruned "
        << inputConstraintSize << " out of "
        << pathConstraintSize << " constraints\n\n";
}

} // namespace plugins
} // namespace s2e
