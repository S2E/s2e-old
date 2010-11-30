#ifndef _CBASIC_BLOCK_H_

#define _CBASIC_BLOCK_H_

#include <llvm/Function.h>


namespace s2etools {
namespace translator {

class CBasicBlock {
private:
    llvm::Function *m_function;
    uint64_t m_address;
    unsigned m_size;

    void markInstructionBoundaries();

public:
    CBasicBlock(llvm::Function *f, uint64_t va, unsigned size);
    ~CBasicBlock();

    const llvm::Function *getFunction() const {
        return m_function;
    }

};

}
}

#endif
