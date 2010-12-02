#ifndef _CBASIC_BLOCK_H_

#define _CBASIC_BLOCK_H_

#include <llvm/Function.h>
#include <llvm/Instructions.h>
#include <ostream>

namespace s2etools {
namespace translator {

enum EBasicBlockType
{
    BB_DEFAULT=0,
    BB_JMP, BB_JMP_IND,
    BB_COND_JMP, BB_COND_JMP_IND,
    BB_CALL, BB_CALL_IND, BB_REP, BB_RET
};


class CBasicBlock {
public:
    //List of basic block addresses that follow this block
    typedef std::set<uint64_t> Successors;
    typedef std::set<uint64_t> AddressSet;
    typedef std::map<uint64_t, llvm::CallInst*> Markers;

private:
    llvm::Function *m_function;
    uint64_t m_address;
    unsigned m_size;
    EBasicBlockType m_type;
    Successors m_successors;
    Markers m_instructionMarkers;

    //Next unique id for cloned function
    static unsigned s_seqNum;

    void markInstructionBoundaries();
    void markTerminator();

    llvm::Function* cloneFunction();

    CBasicBlock();

    void updateInstructionMarkers();
public:
    CBasicBlock(llvm::Function *f, uint64_t va, unsigned size, EBasicBlockType type);
    CBasicBlock(uint64_t va, unsigned size);
    ~CBasicBlock();

    llvm::Function *getFunction() const {
        return m_function;
    }

    EBasicBlockType getType() const {
        return m_type;
    }

    const Successors& getSuccessors() const {
        return m_successors;
    }

    uint64_t getAddress() const {
        return m_address;
    }

    unsigned getSize() const {
        return m_size;
    }

    CBasicBlock* split(uint64_t va);

    void toString(std::ostream &os) const;

    bool valid() const;

    void intersect(CBasicBlock *bb1, AddressSet &result) const;

    bool isReturn() const {
        return m_type == BB_RET;
    }

    bool isIndirectJump() const {
        return m_type == BB_COND_JMP_IND || m_type == BB_JMP_IND;
    }

    bool getInstructionCount() const {
        return m_instructionMarkers.size();
    }

};

struct BasicBlockComparator {
    bool operator()(const CBasicBlock* b1, const CBasicBlock *b2) {
        return b1->getAddress() + b1->getSize() <= b2->getAddress();
    }
};

typedef std::set<translator::CBasicBlock*, BasicBlockComparator> BasicBlocks;


}
}

#endif
