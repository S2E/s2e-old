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
    typedef std::multimap<uint64_t, llvm::Instruction*> MultiInstructions;
    typedef std::map<llvm::Function*, uint64_t> FunctionAddressMap;

    static char ID;
    FunctionBuilder() : ModulePass((intptr_t)&ID) {
        m_entryPoint = NULL;
        m_function = NULL;
    }

    FunctionBuilder(llvm::Function *entryPoint,
                    FunctionAddressMap &basicBlocks,
                    const std::string &functionName) : ModulePass((intptr_t)&ID) {
        m_functionName = functionName;
        m_entryPoint = entryPoint;
        m_function = NULL;
        m_basicBlocks = basicBlocks;
    }

private:
    static std::string TAG;

    llvm::Function *m_entryPoint;
    std::string m_functionName;
    llvm::Function *m_function;
    FunctionAddressMap m_basicBlocks;

    llvm::Function *createFunction(llvm::Module &M);
    void getCalledBbsAndInstructionBoundaries(
            llvm::Module &M, llvm::Function *f,
            Instructions &boundaries,
            MultiInstructions &calledBbs
         );

    void patchCallMarkersWithRealFunctions();

    void patchJumps(Instructions &boundaries,
                    MultiInstructions &branchTargets);


public:
    virtual bool runOnModule(llvm::Module &M);

    llvm::Function* getFunction() const {
        return m_function;
    }


};


#endif
