#include <s2e/S2E.h>
#include <s2e/Utils.h>
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
    s2e()->getMessagesStream()
            << "InputGenerator: processTestCase of state " << state->getID()
            << " at address " << hexval(state->getPc())
            << '\n';

    klee::ExecutionState* exploitState =
                new klee::ExecutionState(*state);
    pruneInputConstraints(state, exploitState);

}

void InputGenerator::pruneInputConstraints(
        S2EExecutionState *state,
        klee::ExecutionState *exploitState)
{
    std::vector< klee::ref<klee::Expr> >
        pathConstraints(state->constraints.size());
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
        << state->inputConstraints.size() << " out of "
        << state->constraints.size() << " constraints\n\n";
}

} // namespace plugins
} // namespace s2e
