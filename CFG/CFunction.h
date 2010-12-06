#ifndef _CFUNCTION_H_

#define _CFUNCTION_H_

#include <llvm/Function.h>
#include <llvm/Instructions.h>
#include <ostream>
#include <map>

#include "CBasicBlock.h"

namespace s2etools {
namespace translator {

typedef std::map<llvm::Function*, uint64_t> FunctionAddressMap;

class CFunction {

private:
    CBasicBlock *m_entryPoint;
    llvm::Function *m_function;

    void valid();

public:
    CFunction(CBasicBlock *entryPoint) {
        m_entryPoint = entryPoint;
        m_function = NULL;
    }

    void generate(FunctionAddressMap &fcnAddrMap);

    llvm::Function *getFunction() const {
        return m_function;
    }

    uint64_t getAddress() const {
        return m_entryPoint->getAddress();
    }

};

struct FunctionComparator {
    bool operator()(const CFunction* b1, const CFunction *b2) {
        return b1->getAddress() < b2->getAddress();
    }
};

typedef std::set<translator::CFunction*, FunctionComparator> CFunctions;



}
}

#endif
