#include <sstream>
#include <llvm/TypeSymbolTable.h>

#include "lib/Utils/Utils.h"
#include "CpuStatePatcher.h"
#include "ForcedInliner.h"

using namespace llvm;

namespace s2etools {

char CpuStatePatcher::ID = 0;
LogKey CpuStatePatcher::TAG = LogKey("CpuStatePatcher");

std::string CpuStatePatcher::s_cpuStateTypeName = "struct.CPUX86State";

CpuStatePatcher::CpuStatePatcher(uint64_t instructionAddress) : FunctionPass((intptr_t)&ID) {
    m_instructionAddress = instructionAddress;
}

void CpuStatePatcher::getAllLoadStores(Instructions &I, llvm::Function &F) const
{
    foreach(bit, F.begin(), F.end()) {
        BasicBlock &BB = *bit;
        foreach(iit, BB.begin(), BB.end()) {
            Instruction *instr = &*iit;
            if (dyn_cast<LoadInst>(instr) || dyn_cast<StoreInst>(instr)) {
                I.push_back(instr);
            }
        }
    }
}

void CpuStatePatcher::transformArguments(llvm::Function *transformed,
                                         llvm::Function *original) const
{
    //At this point, the cloned function is empty.
    //We need to create an initial basic block.
    Module *M = transformed->getParent();
    BasicBlock *entryBlock = BasicBlock::Create(M->getContext(), "", transformed, NULL);

    //Create a type cast from CPUState to uint8_t** in order to
    //accomodate the existing function. These casts will be removed when
    //after reconstructing accesses to the CPUState structure.
    Value *cpuState = transformed->arg_begin();

    //%envAddr = alloca %struct.State*
    Value *envAddr = new AllocaInst(cpuState->getType(), "", entryBlock);

    //store %struct.State* %env, %struct.State** %env_addr
    StoreInst *storeAddr = new StoreInst(cpuState, envAddr, entryBlock);

    //%envAddr1 = bitcast %struct.State** %env_addr to i64*
    BitCastInst *envAddr1 = new BitCastInst(envAddr,
                                      PointerType::getUnqual(IntegerType::get(M->getContext(), 64)),
                                      "",
                                      entryBlock);

    //Create a call to the old function with the casted environment
    SmallVector<Value*,1> CallArguments;
    CallArguments.push_back(envAddr1);
    CallInst *newCall = CallInst::Create(original, CallArguments.begin(), CallArguments.end());
    newCall->insertAfter(envAddr1);

    //Terminate the basic block
    Constant *retVal = ConstantInt::get(M->getContext(), APInt(64, 0, false));
    ReturnInst::Create(M->getContext(), retVal, entryBlock);

    LOGDEBUG() << *transformed << std::endl << std::flush;

    //Inline the old function
    ForcedInliner inliner;
    inliner.addFunction(transformed);
    inliner.inlineFunctions();
    original->deleteBody();

    //Now we have an instruction that accepts CPUState as parameter
    //instead of an opaque uint64_t* object.
}

Function *CpuStatePatcher::createTransformedFunction(llvm::Function &original) const
{
    std::stringstream ss;
    ss << "instr_" << std::hex << m_instructionAddress;

    Module *M = original.getParent();
    Function *newInstr = M->getFunction(ss.str());
    assert(!newInstr && "Can patch state only once for each instruction");

    //Fetch the CpuState type
    const Type *cpuStateType = M->getTypeByName(s_cpuStateTypeName);
    assert(cpuStateType && "Structure type CPUX86State not defined. You must link the emulation bitcode library.");

    //Create the function type
    std::vector<const Type *> paramTypes;
    paramTypes.push_back(PointerType::getUnqual(cpuStateType));

    FunctionType *type = FunctionType::get(
            original.getReturnType(), paramTypes, false);

    //Create the function
    newInstr = dyn_cast<Function>(M->getOrInsertFunction(ss.str(), type));

    return newInstr;
}

bool CpuStatePatcher::runOnFunction(llvm::Function &F)
{
    Instructions registerAccesses;

    getAllLoadStores(registerAccesses, F);
    LOGDEBUG() << "There are " << registerAccesses.size() << " register accesses in "
            << F.getNameStr() << std::endl;

    Function *transformed = createTransformedFunction(F);
    transformArguments(transformed, &F);
}

}
