#ifndef REVGEN_CALLING_CONVENTION_H

#define REVGEN_CALLING_CONVENTION_H

#include <llvm/Pass.h>
#include <llvm/Instructions.h>

#include "lib/Utils/Log.h"
#include "lib/X86Translator/Translator.h"

namespace s2etools {
class CallingConvention : public llvm::ImmutablePass {
private:
    static LogKey TAG;

    llvm::Value* CastToInteger(llvm::Value *value, const llvm::IntegerType *resType);

    void generateGuestCallCdecl(llvm::Value *cpuState, llvm::Function *callee,
                           llvm::BasicBlock *insertAtEnd,
                           std::vector<llvm::Value*> &parameters,
                           llvm::Value *stack);

    void generateGuestCall64(llvm::Value *cpuState, llvm::Function *callee,
                           llvm::BasicBlock *insertAtEnd,
                           std::vector<llvm::Value*> &parameters,
                           llvm::Value *stack);

    void push(llvm::BasicBlock *BB, llvm::Value *cpuState, llvm::GetElementPtrInst *stackPtr,
                                 llvm::Value *value);
public:
    enum Convention {
        CallerSave, CalleeSave, Unknown
    };

    enum Abi {
        SYSV64, MS64, UNKNOWNABI
    };

    static char ID;

    CallingConvention() : ImmutablePass(&ID) {
    }

    virtual void getAnalysisUsage(llvm::AnalysisUsage &AU) const;

    Abi getAbi() const;

    Convention getConvention(const llvm::GetElementPtrInst *reg) const;
    void generateGuestCall(llvm::Value *cpuState, llvm::Function *callee,
                           llvm::BasicBlock *insertAtEnd,
                           std::vector<llvm::Value*> &parameters,
                           llvm::Value *stack);

};
}

#endif
