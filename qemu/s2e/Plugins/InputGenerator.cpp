#include <string>
#include <s2e/S2E.h>
#include <s2e/Utils.h>
#include <s2e/S2EExecutor.h>
#include <s2e/ConfigFile.h>
#include "InputGenerator.h"

namespace s2e {
namespace plugins {

S2E_DEFINE_PLUGIN(InputGenerator, "Tutorial - Generating inputs", "InputGenerator",);

int num = 1;
int solvedNum = 0;

void InputGenerator::initialize()
{
    testCaseNum = s2e()->getConfig()->getInt(getConfigKey() + ".testCaseNum");
    algorithm = s2e()->getConfig()->getInt(getConfigKey() + ".algorithm");

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

    generateArgsConstraints(state, exploitState);
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

void InputGenerator::generateArgsConstraints(
        S2EExecutionState *state,
        klee::ExecutionState *exploitState)
{
    klee::ref<klee::Expr> byte, argsConstraints;

    s2e()->getDebugStream(state) << "========== Argument Constraints ==========\n";
    s2e()->getDebugStream(state) << "Argument Constraints Vector Size: " << argsConstraintVectorSize << "\n";
    for (int i = 0; i < argsConstraintVectorSize; i++) {
        s2e()->getDebugStream(state) << "type: " << state->argsConstraintsType[i] << '\n';
        for(int j = 0; j < state->argsConstraintsAll[i].size(); j++)
        {
            s2e()->getDebugStream(state) << state->argsConstraintsAll[i][j] << '\n';
        }
        s2e()->getDebugStream(state) << "Argument Constraint Size: " << state->argsConstraintsAll[i].size() << "\n\n";

        if(state->argsConstraintsType[i].compare("malloc_size") == 0) {
            s2e()->getDebugStream(state) << "generate args constraint " << '\n';
            klee::ref<klee::Expr> byte, argsConstraints;

            argsConstraints = klee::ConstantExpr::create(0x1, klee::Expr::Bool);

            for(int j = 0; j < state->argsConstraintsAll[i].size(); j++)
            {
                byte = klee::EqExpr::create(state->argsConstraintsAll[i][j],
                        klee::ConstantExpr::create(0x0, klee::Expr::Int32));
                //s2e()->getDebugStream(state) << byte << '\n';
                argsConstraints = klee::AndExpr::create(argsConstraints, byte);
            }
            generateCombination(state, exploitState, argsConstraints);
        }
        if(state->argsConstraintsType[i].compare("strncpy_src") == 0) {
            s2e()->getDebugStream(state) << "generate args constraint " << '\n';
            klee::ref<klee::Expr> byte, argsConstraints;

            argsConstraints = klee::ConstantExpr::create(0x1, klee::Expr::Bool);

            for(int j = 0; j < state->argsConstraintsAll[i].size(); j++)
            {
                if(isa<klee::ConstantExpr>(state->argsConstraintsAll[i][j])) {
                    continue;
                }
                byte = klee::EqExpr::create(state->argsConstraintsAll[i][j],
                        klee::ConstantExpr::create(0x61, klee::Expr::Int32));
                argsConstraints = klee::AndExpr::create(argsConstraints, byte);
            }
            generateCombination(state, exploitState, argsConstraints);
        }
        if(state->argsConstraintsType[i].compare("strcpy_src") == 0) {
            s2e()->getDebugStream(state) << "generate args constraint " << '\n';
            klee::ref<klee::Expr> byte, argsConstraints;

            argsConstraints = klee::ConstantExpr::create(0x1, klee::Expr::Bool);

            for(int j = 0; j < state->argsConstraintsAll[i].size(); j++)
            {
                if(isa<klee::ConstantExpr>(state->argsConstraintsAll[i][j])) {
                    continue;
                }
                byte = klee::EqExpr::create(state->argsConstraintsAll[i][j],
                        klee::ConstantExpr::create(0x61, klee::Expr::Int32));
                argsConstraints = klee::AndExpr::create(argsConstraints, byte);
            }
            generateCombination(state, exploitState, argsConstraints);
        }
        if(state->argsConstraintsType[i].compare("syslog_format") == 0 || state->argsConstraintsType[i].compare("vfprintf_format") == 0) {
            s2e()->getDebugStream(state) << "generate args constraint " << '\n';
            klee::ref<klee::Expr> byte, argsConstraints;

            argsConstraints = klee::ConstantExpr::create(0x1, klee::Expr::Bool);

            for(int j = 0; j < state->argsConstraintsAll[i].size(); j++)
            {
                if(isa<klee::ConstantExpr>(state->argsConstraintsAll[i][j])) {
                    continue;
                }
                byte = klee::EqExpr::create(state->argsConstraintsAll[i][j],
                        klee::ConstantExpr::create(0x61, klee::Expr::Int32));
                s2e()->getDebugStream(state) << byte << '\n';
                argsConstraints = klee::AndExpr::create(argsConstraints, byte);
            }
            generateCombination(state, exploitState, argsConstraints);
        }
    }
}

void InputGenerator::generateCombination(
        S2EExecutionState *state,
        klee::ExecutionState *exploitState,
        klee::ref<klee::Expr> argsConstraints)
{
    // use all path and args constraint to slove.
    klee::ExecutionState* tmpState =
            new klee::ExecutionState(*exploitState);
    tmpState->constraints.constraints.push_back(argsConstraints);
    generateCrash(state, tmpState);
    s2e()->getDebugStream() << "Combination = All" << "\n\n";

    if (num > 1) {
        // sloved => no need scheduling.
        exit(0);
    }

    s2e()->getWarningsStream() << "Start scheduling..." << "\n\n";

    switch(algorithm) {
        case 1 :
            CombinationFromHead(state, exploitState, argsConstraints);
            break;
        case 2 :
            CombinationFromEnd(state, exploitState, argsConstraints);
            break;
        default :
            break;
    }

}

void InputGenerator::CombinationFromHead(
        S2EExecutionState *state,
        klee::ExecutionState *exploitState,
        klee::ref<klee::Expr> argsConstraints)
{
    int n = exploitState->constraints.constraints.size();
    s2e()->getDebugStream(state) << "C(n,r) n = " << n << '\n';

    for (int r = 0; r <= n; r++) {
        std::vector<bool> v(n);
        std::fill(v.begin() + r, v.end(), true);

        klee::ExecutionState* tmpState =
                new klee::ExecutionState(*exploitState);
        tmpState->constraints.constraints.clear();

        do {
            std::stringstream ss;
            ss << "Combination = ";
            if (r == 0)
            {
                ss << "None";
            }
            for (int i = 0; i < n; ++i) {
                if (!v[i]) {
                    ss << (i+1) << " ";
                    tmpState->constraints.constraints.push_back(exploitState->constraints.constraints[i]);
                }
            }
            tmpState->constraints.constraints.push_back(argsConstraints);
            generateCrash(state, tmpState);
            s2e()->getDebugStream(state) << ss.str() << '\n';
            tmpState->constraints.constraints.clear();
        } while (std::next_permutation(v.begin(), v.end()));
    }
}

void InputGenerator::CombinationFromEnd(
        S2EExecutionState *state,
        klee::ExecutionState *exploitState,
        klee::ref<klee::Expr> argsConstraints)
{
    int n = exploitState->constraints.constraints.size();
    s2e()->getDebugStream(state) << "C(n,r) n = " << n << '\n';

    for (int r = 0; r <= n; r++) {
        std::vector<bool> v(n);
        std::fill(v.begin() + n - r, v.end(), true);

        klee::ExecutionState* tmpState =
                new klee::ExecutionState(*exploitState);
        tmpState->constraints.constraints.clear();

        do {
            std::stringstream ss;
            ss << "Combination = ";
            if (r == 0)
            {
                ss << "None";
            }
            for (int i = 0; i < n; ++i) {
                if (v[i]) {
                    ss << (i+1) << " ";
                    tmpState->constraints.constraints.push_back(exploitState->constraints.constraints[i]);
                }
            }
            tmpState->constraints.constraints.push_back(argsConstraints);
            generateCrash(state, tmpState);
            s2e()->getDebugStream(state) << ss.str() << '\n';
            tmpState->constraints.constraints.clear();
        } while (std::next_permutation(v.begin(), v.end()));
    }
}

void InputGenerator::generateCrash(
        S2EExecutionState *state,
        klee::ExecutionState *exploitState)
{
    /*s2e()->getDebugStream() << "========== Exploit Constraints ==========\n";
    for (int i = 0; i < exploitState->constraints.constraints.size(); i++) {
        s2e()->getDebugStream() << exploitState->constraints.constraints[i] << '\n';
    }
    s2e()->getDebugStream() << "Exploit Constraint Size: " << exploitState->constraints.constraints.size() << "\n\n";*/

    solvedNum++;

    struct stat filestat;
    //Get file stat
    if ( lstat(s2e()->getOutputFilename("result.txt").c_str(), &filestat) == 0) {
        solvedNum--;
        s2e()->getWarningsStream() << "Try " << solvedNum << " combinations\n";
        s2e()->getWarningsStream() << "Verify done." << '\n';
        exit(0);
    }

    if (num > testCaseNum) {
        solvedNum--;
        s2e()->getWarningsStream() << "Try " << solvedNum << " combinations\n";
        s2e()->getWarningsStream() << "Generated " << testCaseNum << " TestCase.\n";
        exit(0);
    }

    ConcreteInputs out;
    bool success = s2e()->getExecutor()->getSymbolicSolution(*exploitState, out);

    if (!success) {
        s2e()->getWarningsStream() << "Could not get symbolic solutions" << '\n';
        return;
    }

    s2e()->getMessagesStream() << '\n';

    std::stringstream ss;
    std::stringstream filename;
    filename << "TestCase_" << num << ".bin";
    s2e()->getMessagesStream(state) << "Write TestCase to file " << filename.str() << '\n';
    std::ofstream fout(s2e()->getOutputFilename(filename.str()).c_str(),std::ios::out | std::ios::binary);
    ConcreteInputs::iterator it;
    for (it = out.begin(); it != out.end(); ++it) {
        const VarValuePair &vp = *it;
        ss << std::setw(20) << vp.first << ": ";

        for (unsigned i = 0; i < vp.second.size(); ++i) {
            if (i != 0)
                ss << ' ';
            ss << std::setw(2) << std::setfill('0') << std::hex << (unsigned) vp.second[i] << std::dec;
            fout.put((unsigned) vp.second[i]);
        }
        ss << std::setfill(' ') << ", ";
        fout.put(' ');

        if (vp.second.size() == sizeof(int32_t)) {
            int32_t valueAsInt = vp.second[0] | ((int32_t)vp.second[1] << 8) | ((int32_t)vp.second[2] << 16) | ((int32_t)vp.second[3] << 24);
            ss << "(int32_t) " << valueAsInt << ", ";
        }
        if (vp.second.size() == sizeof(int64_t)) {
            int64_t valueAsInt = vp.second[0] | ((int64_t)vp.second[1] <<  8) | ((int64_t)vp.second[2] << 16) | ((int64_t)vp.second[3] << 24) |
                ((int64_t)vp.second[4] << 32) | ((int64_t)vp.second[5] << 40) | ((int64_t)vp.second[6] << 48) | ((int64_t)vp.second[7] << 56);
            ss << "(int64_t) " << valueAsInt << ", ";
        }

        ss << "(string) \"";
        for (unsigned i=0; i < vp.second.size(); ++i) {
            ss << (char)(std::isprint(vp.second[i]) ? vp.second[i] : '.');
        }
        ss << "\"\n";
    }
    num++;
    fout.close();

    s2e()->getMessagesStream() << ss.str();
}

} // namespace plugins
} // namespace s2e
