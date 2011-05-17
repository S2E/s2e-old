#ifndef _INSTR_FUNCTIONBUILDER_PASS_H_

#define _INSTR_FUNCTIONBUILDER_PASS_H_

#include "llvm/Pass.h"
#include "llvm/Function.h"
#include "llvm/Module.h"
#include "llvm/Instructions.h"

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"


#include "lib/Utils/Log.h"
#include "lib/Utils/Utils.h"
#include "lib/X86Translator/Translator.h"

#include <set>
#include <map>
#include <string>
#include <vector>

namespace s2etools {

/**
 * This pass takes a list of basic blocks in LLVM format and reconstructs
 * a function out of them.
 */
struct FunctionBuilder : public llvm::ModulePass {
    typedef llvm::DenseMap<uint64_t, TranslatedBlock *> TranslatedBlocksMap;
    typedef llvm::DenseSet<uint64_t> AddressSet;
    typedef std::vector<uint64_t> AddressVector;
    typedef llvm::DenseMap<uint64_t, llvm::BasicBlock*> BasicBlocksMap;
    static char ID;
#if 0
    FunctionBuilder() : ModulePass((intptr_t)&ID) {
        m_entryPoint = NULL;
        m_function = NULL;
    }
#endif

    FunctionBuilder(const TranslatedBlocksMap &allInstructions,
                    const AddressSet &functionInstructions,
                    uint64_t entryPoint) : ModulePass((intptr_t)&ID),
                    m_allInstructions(allInstructions),
                    m_functionInstructions(functionInstructions)
    {
        m_entryPoint = entryPoint;
        m_function = NULL;
    }

private:
    static LogKey TAG;
    llvm::Function *m_function;
    uint64_t m_entryPoint;
    const TranslatedBlocksMap &m_allInstructions;
    const AddressSet &m_functionInstructions;

    //Pointer to the environment
    llvm::Instruction *m_env;

    //GEP instruction for the program counter
    llvm::Instruction *m_pcptr;

    AddressSet m_basicBlockBoundaries;

    void buildPcGep(llvm::Module &M, llvm::BasicBlock *bb);
    void buildConditionalBranch(llvm::Module &M, BasicBlocksMap &bbmap, llvm::BasicBlock *bb, TranslatedBlock *tb);
    void buildIndirectConditionalBranch(llvm::Module &M, BasicBlocksMap &bbmap, llvm::BasicBlock *bb, TranslatedBlock *tb);
    void buildUnconditionalBranch(llvm::Module &M, BasicBlocksMap &bbmap, llvm::BasicBlock *bb, TranslatedBlock *tb);
    void insertJumpMarker(llvm::Module &M, llvm::BasicBlock *bb, llvm::Value *target);

    void createFunction(llvm::Module &M);
    void computeBasicBlockBoundaries();
    void buildFunction(llvm::Module &M);
    void callInstruction(llvm::BasicBlock *bb, llvm::Function *f);


public:
    virtual bool runOnModule(llvm::Module &M);

    llvm::Function* getFunction() const {
        return m_function;
    }


};

}


#endif
