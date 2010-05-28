#include <s2e/S2E.h>
#include <s2e/S2EExecutionState.h>
#include <s2e/S2EExecutor.h>
#include "TestCaseGenerator.h"

namespace s2e {
namespace plugins {

S2E_DEFINE_PLUGIN(TestCaseGenerator, "TestCaseGenerator plugin", "TestCaseGenerator",);

TestCaseGenerator::TestCaseGenerator(S2E* s2e)
        : Plugin(s2e)
{
    m_testIndex = 0;
    m_pathsExplored = 0;

}

void TestCaseGenerator::initialize()
{
    //ConfigFile* conf = s2e()->getConfig();
}


void TestCaseGenerator::processTestCase(const S2EExecutionState &state,
                     const char *err, const char *suffix)
{
    s2e()->getMessagesStream()
            << "TestCaseGenerator: processTestCase of state " << state.getID() << std::endl;

    ConcreteInputs out;
    bool success = s2e()->getExecutor()->getSymbolicSolution(state, out);

    if (!success) {
        s2e()->getWarningsStream() << "Could not get symbolic solutions" << std::endl;
        return;
    }

    ConcreteInputs::iterator it;
    for (it = out.begin(); it != out.end(); ++it) {
        const VarValuePair &vp = *it;
        s2e()->getMessagesStream() << vp.first << ": " << std::endl;

        for (unsigned i=0; i<vp.second.size(); ++i) {
            s2e()->getMessagesStream() << vp.second[i];
        }

        s2e()->getMessagesStream() << std::endl;
    }
}

}
}
