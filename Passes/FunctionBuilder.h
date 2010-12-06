#ifndef _INSTR_TBCUTTER_PASS_H_

#define _INSTR_TBCUTTER_PASS_H_

#include "llvm/Pass.h"
#include "llvm/Function.h"
#include "llvm/Module.h"
#include "llvm/Instructions.h"

#include <set>
#include <map>
#include <string>

/**
 * This pass takes a list of basic blocks in LLVM format and reconstructs
 * a function out of them.
 */
struct FunctionBuilder : public llvm::ModulePass {
    typedef std::set<llvm::Function*> Functions;
    typedef std::map<uint64_t, llvm::Instruction*> Instructions;

    static char ID;
    FunctionBuilder() : ModulePass((intptr_t)&ID) {

    }

    FunctionBuilder(llvm::Function *entryPoint,
                    const std::string &functionName) : ModulePass((intptr_t)&ID) {
        m_functionName = functionName;
        m_entryPoint = entryPoint;
    }

public:

private:

    llvm::Function *m_entryPoint;
    std::string m_functionName;

    llvm::Function *createFunction(llvm::Module &M);
    void getCalledBbsAndInstructionBoundaries(
            llvm::Module &M, llvm::Function *f,
            Instructions &boundaries,
            Instructions &calledBbs
         );

    void patchJumps(Instructions &boundaries,
                    Instructions &branchTargets);

public:
    virtual bool runOnModule(llvm::Module &M);



};


#endif
