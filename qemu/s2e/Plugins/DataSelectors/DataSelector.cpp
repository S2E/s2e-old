#include <s2e/S2E.h>
#include <s2e/ConfigFile.h>
#include <s2e/Utils.h>
#include <sstream>  
#include "DataSelector.h"

using namespace s2e;
using namespace plugins;
using namespace klee;

void DataSelector::initialize()
{
    //Check that the interceptor is there
    m_ExecDetector = (ModuleExecutionDetector*)s2e()->getPlugin("ModuleExecutionDetector");
    assert(m_ExecDetector);

  
    std::vector<std::string> Sections;
    Sections = s2e()->getConfig()->getListKeys(getConfigKey());
    bool noErrors = true;

    if (Sections.size() > 1) {
        s2e()->getWarningsStream() << "Only one service can be handled currently..." << std::endl;
        exit(-1);
    }

    foreach2(it, Sections.begin(), Sections.end()) {
        s2e()->getMessagesStream() << "Scanning section " << getConfigKey() << "." << *it << std::endl;
        std::stringstream sk;
        sk << getConfigKey() << "." << *it;
        if (!initSection(sk.str(), *it)) {
            noErrors = false;
        }
    }

    if (!noErrors) {
        s2e()->getWarningsStream() << "Errors while scanning the WindowsService sections"
            <<std::endl;
        exit(-1);
    }

}

ref<Expr> DataSelector::getNonNullCharacter(Expr::Width w)
{
    ref<Expr> symbVal = S2EExecutionState::createSymbolicValue(w);
    return NeExpr::create(symbVal, ConstantExpr::create(0,w));
}

ref<Expr> DataSelector::getUpperBound(uint64_t upperBound, Expr::Width w)
{
    ref<Expr> symbVal = S2EExecutionState::createSymbolicValue(w);
    return UltExpr::create(symbVal, ConstantExpr::create(upperBound,w));
}

bool DataSelector::makeUnicodeStringSymbolic(S2EExecutionState *s, uint64_t address)
{
    do {
        uint16_t car;
        SREADR(s,address,car);

        if (!car) {
            return true;
        }

        ref<Expr> v = getNonNullCharacter(Expr::Int16);
        s2e()->getMessagesStream() << v << std::endl;
        s->writeMemory(address, v); 
        address+=sizeof(car);
    }while(1);
    
    return true;
}

bool DataSelector::makeStringSymbolic(S2EExecutionState *s, uint64_t address)
{
    do {
        uint8_t car;
        SREADR(s,address,car);

        if (!car) {
            return true;
        }

        ref<Expr> v = getNonNullCharacter(Expr::Int8);
        s2e()->getMessagesStream() << v << std::endl;
        s->writeMemory(address, v); 
        address+=sizeof(car);
    }while(1);
    
    return true;
}

klee::ref<klee::Expr> DataSelector::getOddValue(klee::Expr::Width w)
{
    ref<Expr> symbVal = S2EExecutionState::createSymbolicValue(w);
    ref<Expr> e1 = MulExpr::create(ConstantExpr::create(2,w), symbVal);
    ref<Expr> e2 = AddExpr::create(e1, ConstantExpr::create(1,w));
    return e2;
}

klee::ref<klee::Expr> DataSelector::getOddValue(klee::Expr::Width w, uint64_t upperBound)
{
    ref<Expr> e1 = getOddValue(w);
    return UltExpr::create(e1, ConstantExpr::create(upperBound,w));
}
