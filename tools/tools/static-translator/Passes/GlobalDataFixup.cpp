#include <llvm/Module.h>
#include <llvm/Function.h>
#include <llvm/BasicBlock.h>
#include <llvm/Constants.h>
#include <llvm/Support/InstIterator.h>

#include "lib/Utils/Log.h"
#include "GlobalDataFixup.h"
#include "lib/Utils/Utils.h"

#include <set>
#include <sstream>
#include <iostream>

using namespace llvm;
using namespace s2etools;

char GlobalDataFixup::ID = 0;
RegisterPass<GlobalDataFixup>
  GlobalDataFixup("GlobalDataFixup", "Fixes all references to global data",
  false /* Only looks at CFG */,
  false /* Analysis Pass */);

std::string GlobalDataFixup::TAG="GlobalDataFixup";

/**
 * Creates a global array initialized with the content of data.
 * Initialized to zero if data is null.
 */
void GlobalDataFixup::createGlobalArray(llvm::Module &M, const std::string &name, uint8_t *data, uint64_t va, unsigned size)
{
    std::stringstream ss;
    ss << name << "_global_" << std::hex << va;

    std::vector<Constant*> constants;

    if (data) {
        //Only if we have an initialized section
        for(unsigned i=0; i<size; ++i) {
            constants.push_back(ConstantInt::get(M.getContext(), APInt(8,  data[i])));
        }
    }

    GlobalVariable *var = M.getGlobalVariable(ss.str(), false);
    assert(!var);


    ArrayType *type = ArrayType::get(Type::getInt8Ty(M.getContext()), size);
    Constant *initializer = NULL;

    if (data) {
        initializer = ConstantArray::get(type, constants);
    }else {
        initializer = ConstantAggregateZero::get(type);
    }


    var = new GlobalVariable(M, type, false,
                             llvm::GlobalVariable::PrivateLinkage , initializer, ss.str());
    BFDSection sec;
    sec.start = va;
    sec.size = size;
    m_sections[sec] = var;

    LOGDEBUG() << *var;
}

/**
 * Stores all data sections of the binary into global arrays
 */
void GlobalDataFixup::injectDataSections(llvm::Module &M)
{
    const BFDInterface::Sections &sections = m_binary->getSections();

    foreach(it, sections.begin(), sections.end()) {
        int flags = m_binary->getSectionFlags((*it).first.start);
        uint64_t start = (*it).first.start;
        uint64_t size = (*it).first.size;

        LOGINFO() << "Processing section " << (*it).second->name << " flags=0x" << std::hex << flags << std::endl;

        if (flags & SEC_DEBUGGING) {
            continue;
        }

        if (flags & SEC_HAS_CONTENTS) {
            uint8_t *sec = new uint8_t[size];
            bool b = m_binary->read(start, sec, size);
            assert(b);
            createGlobalArray(M, (*it).second->name, sec, start, size);
            delete sec;
        }else {
            //This is BSS section
            if (flags & SEC_ALLOC) {
                createGlobalArray(M, (*it).second->name, 0, start, size);
            }
        }
    }
}


void GlobalDataFixup::initMemOps(Module &M)
{
    const char *builtinFunctionsStr[] = {
                                     "__ldb_mmu", "__ldw_mmu", "__ldl_mmu", "__ldq_mmu",
                                     "__stb_mmu", "__stw_mmu", "__stl_mmu", "__stq_mmu"};


    for (unsigned i=0; i<sizeof(builtinFunctionsStr)/sizeof(builtinFunctionsStr[0]); ++i) {
        Function *fcn = M.getFunction(builtinFunctionsStr[i]);
        assert(fcn);
        unsigned size = 1 << (i % 4);
        bool isLoad = i < 4;
        m_memops.insert(std::make_pair(fcn, std::make_pair(size, isLoad)));
    }
}

bool GlobalDataFixup::patchFunctionPointer(Module &M, Value *ptr)
{
    ConstantInt *addr = dyn_cast<ConstantInt>(ptr);
    assert(addr);
    const uint64_t target = *addr->getValue().getRawData();

    Function *function;
    std::string name;

    //Check whether we have an imported function
    const Imports &imports = m_binary->getImports();
    Imports::const_iterator it = imports.find(target);
    if (it != imports.end()) {
        name = (*it).second.second;
        function = M.getFunction(name);
        if (!function) {
            LOGERROR() << "Could not find imported function " << name << std::endl;
            LOGERROR() << "Check that the bitcode library declares it as an external" << std::endl;
            assert(false);
        }

        if (!function->isDeclaration()) {
            LOGERROR() << "Imported function " << name << " must not be defined by the bitcode library" << std::endl;
            assert(false);
        }

    }else {
        //Check if it is an internal function
        std::stringstream ss;
        ss << "function_" << std::hex << target;
        name = ss.str();
        function = M.getFunction(name);
        if (!function) {
            return false;
        }
    }

    assert(function);

    //Cast the pointer to integer
    ptr->replaceAllUsesWith(ConstantExpr::getPtrToInt(function, ptr->getType()));
    return true;
}

bool GlobalDataFixup::patchImportedDataPointer(Module &M,
                                               Instruction *instructionMarker,
                                               Value *ptr, const RelocationEntry &relEntry)
{
    ConstantInt *addr = dyn_cast<ConstantInt>(ptr);
    assert(addr);
    //const uint64_t target = *addr->getValue().getRawData();

    if (!relEntry.symbolBase) {
        return false;
    }

    const Imports &imports = m_binary->getImports();
    Imports::const_iterator it = imports.find(relEntry.symbolBase);
    if (it == imports.end()) {
        return false;
    }

    //std::stringstream mangledNameS;
    //mangledNameS << (*it).second.first << "_" << (*it).second.second;
    //std::string name = mangledNameS.str();

    //The imported symbols must also be imported by the Bitcode library.
    //The symbols must not be redefined (otherwise linking problems...)

    //Remove the leading underscore
    //XXX: should be put in a central place...
    std::string name = (*it).second.second.substr(1);

    //foreach(it, M.global_begin(), M.global_end()) {
    //    std::cout << (*it).getNameStr() << std::endl;
    //}

    GlobalVariable *importedVariable = M.getNamedGlobal(name);
    if (!importedVariable) {
        return false;
    }

    //Add the offset to the base of the variable (to accomodate arrays)
    Value *V = ConstantExpr::getPtrToInt(importedVariable, ptr->getType());
    uint32_t sizeInBits = V->getType()->getScalarSizeInBits();
    Constant *offset = ConstantInt::get(V->getType(), APInt(sizeInBits, relEntry.getOffetFromSymbol()));
    Instruction *finalPointer = BinaryOperator::CreateAdd(V, offset);
    finalPointer->insertAfter(instructionMarker);
    //Cast the pointer to integer
    ptr->replaceAllUsesWith(finalPointer);
    return true;
}


//Pointer whithin the section
void GlobalDataFixup::patchDataPointer(Module &M, Value *ptr)
{
    ConstantInt *addr = dyn_cast<ConstantInt>(ptr);
    assert(addr);
    const uint64_t pointer = *addr->getValue().getRawData();


    //Fetch the section referenced by pointer
    BFDSection sec;
    sec.size = 1;
    sec.start = pointer;

    Sections::iterator it = m_sections.find(sec);
    if (it == m_sections.end()) {
        LOGERROR() << "Invalid address: 0x" << std::hex << pointer << std::endl;
        assert(false && "Program tries to access a hard-coded address that is not in any section of its binary.");
        return;
    }

    GlobalVariable *var = (*it).second;

    //Create a GEP instruction to the right address
    Constant *gepIndexes[2];
    gepIndexes[0] = ConstantInt::get(M.getContext(), APInt(32,  0));
    gepIndexes[1] = ConstantInt::get(M.getContext(), APInt(32,  pointer - (*it).first.start));
    Constant *gepAddr = ConstantExpr::getInBoundsGetElementPtr(var, gepIndexes, 2);

    //Cast the pointer to integer
    ptr->replaceAllUsesWith(ConstantExpr::getPtrToInt(gepAddr, ptr->getType()));
}


void GlobalDataFixup::patchRelocations(Module &M)
{
    const RelocationEntries &relocEntries = m_binary->getRelocations();

    ProgramCounters counters;
    getProgramCounters(M, counters);
    assert((counters.size() > 0) && "There are no program counter markers in the module");


    foreach(it, relocEntries.begin(), relocEntries.end()) {
        //Fetch the instruction that encloses this relocation entry
        uint64_t relocAddress = (*it).first;
        const RelocationEntry &relEntry = (*it).second;

        //...get the instruction right after the address to be relocated
        ProgramCounters::iterator pcIt = counters.upper_bound(relocAddress);

        //...decrement the iterator to get the right instruction
        --pcIt;
        assert((*pcIt).first <= relocAddress);

        //Scan the function to fetch the LLVM constant value
        Instruction *owner;
        ConstantInt *cste = findValue((*pcIt).second, relEntry.targetValue, &owner);
        assert(cste && "Could not find value to relocate in LLVM stream. Bad.");

        //Determine the type of the target value (function pointer, data, external...)
        bool b;
        b = patchImportedDataPointer(M, (*pcIt).second, cste, relEntry);
        if (b) {
            continue;
        }

        b = patchFunctionPointer(M, cste);
        if (b) {
            continue;
        }

        patchDataPointer(M, cste);
    }
}

//All constants involved in pointers should be present in the relocation table,
//XXX: this would work only for shared libraries in the general case
#if 0
void GlobalDataFixup::patchConstantMemoryAddresses(Module &M, std::set<CallInst *> memOps)
{
    //For each call site, patch the pointer with the address of
    //the global variable.
    foreach(it, memOpsCalls.begin(), memOpsCalls.end()) {
        CallInst *ci = *it;
        unsigned size = (*m_memops.find(ci->getCalledFunction())).second.first;
        bool isLoad = (*m_memops.find(ci->getCalledFunction())).second.second;
        patchAddress(M, *it, isLoad, size);
    }
}

void GlobalDataFixup::patchConstantMemoryAddress(Module &M, CallInst *ci, bool isLoad, unsigned accessSize) {

    LOGERROR() << *ci << std::endl;
    ConstantInt *addr = dyn_cast<ConstantInt>(ci->getOperand(1));
    assert(addr);
    const uint64_t target = *addr->getValue().getRawData();

    //Fetch the corresponding section
    BFDSection sec;
    sec.size = 1;
    sec.start = target;

    Sections::iterator it = m_sections.find(sec);
    if (it == m_sections.end()) {
        assert(false && "Program tries to access a hard-coded address that is not in any section of its binary.");
        return;
    }

    if (target - (*it).first.start + accessSize > (*it).first.size) {
        assert(false && "Data access overflows data section");
        return;
    }

    GlobalVariable *var = (*it).second;

    //Create a GEP instruction to the right address
    Constant *gepIndexes[2];
    gepIndexes[0] = ConstantInt::get(M.getContext(), APInt(32,  0));
    gepIndexes[1] = ConstantInt::get(M.getContext(), APInt(32,  target - (*it).first.start));
    Constant *gepAddr = ConstantExpr::getInBoundsGetElementPtr(var, gepIndexes, 2);

    //Cast the pointer to the right size
    const Type *elemType = NULL;
    switch(accessSize) {
        case 1: elemType = Type::getInt8Ty(M.getContext()); break;
        case 2: elemType = Type::getInt16Ty(M.getContext()); break;
        case 4: elemType = Type::getInt32Ty(M.getContext()); break;
        case 8: elemType = Type::getInt64Ty(M.getContext()); break;
        default: assert(false && "Invalid access size");
    }

    Constant *bitCast = ConstantExpr::getBitCast(gepAddr, PointerType::getUnqual(elemType));

    //Replace the load/store instruction with a native LLVM instruction
    Instruction *accessInstr;
    if (isLoad) {
        accessInstr = new LoadInst(bitCast, "", ci);
        LOGERROR() << "Created load: " << *accessInstr << std::endl;
        ci->replaceAllUsesWith(accessInstr);
        ci->eraseFromParent();
    }else {
        accessInstr = new StoreInst(ci->getOperand(2), bitCast, false, ci);
        LOGERROR() << "Created store: " << *accessInstr << std::endl;
        //Get rid of the original access, not needed anymore
        ci->eraseFromParent();
    }

    //LOGERROR() << *ci->getParent() << std::endl;
}
#endif

/**
 *  Retrieves all the instruction boundary markers from the module.
 *  Only considers final functions, whose CFG is rebuilt.
 */
void GlobalDataFixup::getProgramCounters(llvm::Module &M, ProgramCounters &counters)
{
    Function *instructionMarker = M.getFunction("instruction_marker");
    assert(instructionMarker);

    foreach(fit, M.begin(), M.end()) {
        Function &F = *fit;
        if (F.getNameStr().find("function_") == std::string::npos) {
            continue;
        }

        foreach(iit, inst_begin(F), inst_end(F)) {
            CallInst *ci = dyn_cast<CallInst>(&*iit);
            if (!ci || ci->getCalledFunction() != instructionMarker) {
                continue;
            }

            ConstantInt *addr = dyn_cast<ConstantInt>(ci->getOperand(1));
            assert(addr);
            const uint64_t target = *addr->getValue().getRawData();
            assert(counters.find(target) == counters.end());
            counters[target] = ci;
        }
    }
}

//Looks for the specified value starting at the given instruction
ConstantInt *GlobalDataFixup::findValue(Instruction *boundary, uint64_t value, Instruction **owner)
{
    ilist<Instruction>::iterator iit(boundary);
    ilist<Instruction>::iterator end(boundary->getParent()->end());

    Function *instructionMarker = boundary->getParent()->getParent()->getParent()->getFunction("instruction_marker");
    assert(instructionMarker);

    //Skip over the start marker
    ++iit;

    while(iit != end) {
        Instruction *instr = &*iit;
        CallInst *ci = dyn_cast<CallInst>(instr);
        if (ci && ci->getCalledFunction() == instructionMarker) {
            break;
        }

        for(unsigned i=0; i<instr->getNumOperands(); ++i) {
            ConstantInt *cste = dyn_cast<ConstantInt>(instr->getOperand(i));
            if (cste) {
                uint64_t foundval =  *cste->getValue().getRawData();
                if (foundval == value) {
                    //Found the value to be relocated, will have to patch it.
                    *owner = instr;
                    return cste;
                }
            }
        }
        ++iit;
    }

    assert(false && "Desired relocation variable not found");
    return NULL;
}





/**
  * Patch all integers that are in the relocation table
  * Addresses falling in the code section:
  *   - if function, replace with function pointer
  *     take into account indirect calls and jumps
  *   - if pointing to data, patch with a pointer to the right array
  *     do not assume that value will be used for load/store (might be involved in arithmetic)
  * Addresses of imported symbols:
  *   - the declaration are already located in the BitCodeLibrary
  *   - get the address of data/function and patch the value
  */

//This can only be run once on the module
bool GlobalDataFixup::runOnModule(llvm::Module &M)
{
    initMemOps(M);
    injectDataSections(M);
    patchRelocations(M);

#if 0
    std::set<CallInst *> memOpsCalls;


    //Get all the call sites to MMU memory accesses
    foreach(fit, M.begin(), M.end()) {
        Function &f = *fit;
        //Skip functions that are not final
        std::cout << "GlobalDataFixup " << f.getNameStr() << std::endl;
        if (f.getNameStr().find("function_") == std::string::npos) {
            continue;
        }

        foreach(iit, inst_begin(f), inst_end(f)) {
            CallInst *ci = dyn_cast<CallInst>(&*iit);
            if (!ci || m_memops.find(ci->getCalledFunction()) == m_memops.end()) {
                continue;
            }
            //Only deal with constant addresses
            if (dyn_cast<ConstantInt>(ci->getOperand(1))) {
                memOpsCalls.insert(ci);
            }
        }

    }
#endif

    return false;
}
