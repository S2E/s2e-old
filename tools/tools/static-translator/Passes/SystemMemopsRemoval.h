#ifndef _INSTR_MEMOPREMOVAL_PASS_H_

#define _INSTR_MEMOPREMOVAL_PASS_H_

#include "llvm/Pass.h"
#include "llvm/Function.h"
#include "llvm/Instructions.h"

#include <set>

/**
 *  This pass inserts a marker in the LLVM bitcode between blocks of
 *  LLVM instructions belonging to the same original x86 machine instruction.
 */
struct SystemMemopsRemoval : public llvm::FunctionPass {
    static char ID;
    SystemMemopsRemoval() : FunctionPass((intptr_t)&ID) {

    }

    virtual void getAnalysisUsage(llvm::AnalysisUsage &AU) const;
private:
    bool getEnvOffsetFromLoadStore(llvm::Instruction *instr, uint64_t &offset);
    bool getEnvOffset(llvm::Instruction *instr, uint64_t &offset);
    bool isHardCodedAddress(llvm::Instruction *instr);

public:
    virtual bool runOnFunction(llvm::Function &F);



};


#endif
