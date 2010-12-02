#include <llvm/Function.h>
#include <llvm/BasicBlock.h>
#include <llvm/Module.h>

#include "JumpTableExtractor.h"
#include "Utils.h"

#include <iostream>

#include <set>

using namespace llvm;

char JumpTableExtractor::ID = 0;
RegisterPass<JumpTableExtractor>
  JumpTableExtractor("JumpTableExtractor", "Extracts jump tables referenced by an indirect branch",
  true /* Only looks at CFG */,
  true /* Analysis Pass */);


Value *JumpTableExtractor::getIndirectCallAddress(llvm::Function &F)
{
    //Find the terminating instruction
    ilist<BasicBlock>::reverse_iterator rbbit(F.end());
    BasicBlock &bb = *rbbit;

    ilist<Instruction>::reverse_iterator riit(bb.end());
    ilist<Instruction>::reverse_iterator riitEnd(bb.begin());

    while(riit != riitEnd) {
        CallInst *ci =  dyn_cast<CallInst>(&*riit);
        if (!(ci && ci->getCalledFunction())) {
            riit++;
            continue;
        }
        if (ci->getCalledFunction() == F.getParent()->getFunction("call_marker")) {
            //Get the indirect call
            Value *address = ci->getOperand(1);
            if (!dyn_cast<ConstantInt>(address)) {
                return address;
            }
        }
        riit++;
    }
    return NULL;
}

/**
 *  Looks for an add involving an immediate value.
 *  XXX: This won't work for obfuscated branches, where some arithmetic evaluation
 *  may be necessary.
 *
 *  %eax_v = load i32* %eax_ptr                     ; <i32> [#uses=1]
 *  %tmp4_v = shl i32 %eax_v, 2                     ; <i32> [#uses=1]
 *  %tmp2_v = add i32 71758, %tmp4_v                ; <i32> [#uses=1]
 *  %tmp0_v = call i32 @__ldl_mmu(i32 %tmp2_v, i32 -1) ; <i32> [#uses=1]
 *  %12 = zext i32 %tmp0_v to i64                   ; <i64> [#uses=2]
 *  call void @call_marker(i64 %12, i1 false)
 */
uint64_t JumpTableExtractor::getOffset(Value *address) const
{
    const ZExtInst *zext = dyn_cast<ZExtInst>(address);
    if (!zext) { return 0; }

    const CallInst *load = dyn_cast<CallInst>(zext->getOperand(0));
    if (!load) { return 0; }

    const Instruction *add = dyn_cast<Instruction>(load->getOperand(1));
    if (!add) { return 0; }

    const ConstantInt *ci = dyn_cast<ConstantInt>(add->getOperand(0));
    if (!ci) { return 0; }

    return *ci->getValue().getRawData();
}

bool JumpTableExtractor::runOnFunction(llvm::Function &F)
{
    std::cerr << "INDIRECT CALL FCN" << std::endl;
    std::cerr << F;

    Value *address = getIndirectCallAddress(F);
    if (!address) {
        return false;
    }

    uint64_t offset = getOffset(address);
    if (!offset) {
        std::cerr << "Could not find jump table (offset not found)" << std::endl;
        return false;
    }

    m_jumpTableAddress = offset;

    return true;
}
