#ifndef S2E_PLUGINS_TCGEN_H
#define S2E_PLUGINS_TCGEN_H

#include <s2e/Plugin.h>
#include <string>

namespace s2e{
namespace plugins{

/** Handler required for KLEE interpreter */
class TestCaseGenerator : public Plugin
{
    S2E_PLUGIN

private:
    typedef std::pair<std::string, std::vector<unsigned char> > VarValuePair;
    typedef std::vector<VarValuePair> ConcreteInputs;

    unsigned m_testIndex;  // number of tests written so far
    unsigned m_pathsExplored; // number of paths explored so far

public:
    TestCaseGenerator(S2E* s2e);

    void initialize();

    /* klee-related function */
    void processTestCase(const S2EExecutionState &state,
                         const char *err, const char *suffix);
};


}
}

#endif
