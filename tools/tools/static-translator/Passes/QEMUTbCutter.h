#ifndef _INSTR_TBCUTTER_PASS_H_

#define _INSTR_TBCUTTER_PASS_H_

#include "llvm/Pass.h"
#include "llvm/Function.h"
#include "llvm/Instructions.h"

#include <set>

/**
 *  This pass inserts a marker in the LLVM bitcode between blocks of
 *  LLVM instructions belonging to the same original x86 machine instruction.
 */
struct QEMUTbCutter : public llvm::FunctionPass {
    static char ID;
    QEMUTbCutter() : FunctionPass((intptr_t)&ID) {

    }

    QEMUTbCutter(llvm::CallInst *marker, bool cutBefore) : FunctionPass((intptr_t)&ID) {
        m_marker = marker;
        m_cutBefore = cutBefore;
    }

public:
    llvm::CallInst *m_marker;
    bool m_cutBefore;
    llvm::Function *m_instructionMarker;

private:
    bool removeBefore(llvm::CallInst *finish);
    bool removeAfter(llvm::CallInst *start);
    void insertUnconditionalBranch(llvm::CallInst *ci);
    bool isInstructionMarker(llvm::CallInst *Ci);

public:
    virtual bool runOnFunction(llvm::Function &F);



};


#endif
