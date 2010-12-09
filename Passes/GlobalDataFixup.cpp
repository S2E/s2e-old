#include <llvm/Module.h>
#include <llvm/Function.h>
#include <llvm/BasicBlock.h>
#include <llvm/Constants.h>
#include <llvm/Support/InstIterator.h>


#include "GlobalDataFixup.h"
#include "Utils.h"

#include <set>
#include <sstream>
#include <iostream>

using namespace llvm;
using namespace s2etools;

char GlobalDataFixup::ID = 0;
RegisterPass<GlobalDataFixup>
  GlobalDataFixup("GlobalDataFixup", "Fixes all references to global data",
  false /* Only looks at CFG */,
  true /* Analysis Pass */);

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

    std::cout << *var;
}

void GlobalDataFixup::injectDataSections(llvm::Module &M)
{
    const BFDInterface::Sections &sections = m_binary->getSections();

    foreach(it, sections.begin(), sections.end()) {
        int flags = m_binary->getSectionFlags((*it).first.start);
        uint64_t start = (*it).first.start;
        uint64_t size = (*it).first.size;

        std::cout << "Processing section " << (*it).second->name << " flags=0x" << std::hex << flags << std::endl;

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

void GlobalDataFixup::patchPointer(Module &M, Value *ptr, Instruction *owner, uint64_t pointer)
{
    //Fetch the corresponding section
    BFDSection sec;
    sec.size = 1;
    sec.start = pointer;

    Sections::iterator it = m_sections.find(sec);
    if (it == m_sections.end()) {
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
    for (unsigned i=0; i<owner->getNumOperands(); ++i) {
        Value *v = owner->getOperand(i);
        if (v == ptr) {
            owner->setOperand(i, ConstantExpr::getPtrToInt(gepAddr, ptr->getType()));
        }
    }
}

void GlobalDataFixup::patchAddress(Module &M, CallInst *ci, bool isLoad, unsigned accessSize) {

    std::cerr << *ci << std::endl;
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
        std::cerr << "Created load: " << *accessInstr << std::endl;
        ci->replaceAllUsesWith(accessInstr);
        ci->eraseFromParent();
    }else {
        accessInstr = new StoreInst(ci->getOperand(2), bitCast, false, ci);
        std::cerr << "Created store: " << *accessInstr << std::endl;
        //Get rid of the original access, not needed anymore
        ci->eraseFromParent();
    }

    //std::cerr << *ci->getParent() << std::endl;
}

void GlobalDataFixup::getProgramCounters(llvm::Function &F, ProgramCounters &counters)
{
    Function *instructionMarker = F.getParent()->getFunction("instruction_marker");
    assert(instructionMarker);

    foreach(iit, inst_begin(F), inst_end(F)) {
        CallInst *ci = dyn_cast<CallInst>(&*iit);
        if (!ci || ci->getCalledFunction() != instructionMarker) {
            continue;
        }

        ConstantInt *addr = dyn_cast<ConstantInt>(ci->getOperand(1));
        assert(addr);
        const uint64_t target = *addr->getValue().getRawData();
        counters[target] = ci;
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

void GlobalDataFixup::loadRelocations(RelocationEntries &result)
{
    //Fetch the corresponding section
    const BFDInterface::Sections &sections = m_binary->getSections();

    foreach (sit, sections.begin(), sections.end()) {
        asection *theSection = (*sit).second;
        if (!(theSection->flags & SEC_RELOC)) {
            //There is nothing to relocate (apparently)
            continue;
        }

        for (unsigned i=0; i<theSection->reloc_count; ++i) {
            uint64_t absAddress = theSection->relocation[i].address + theSection->vma;
            assert(result.find(absAddress) == result.end());
            result[absAddress] = theSection->relocation[i];
        }
    }
}

void GlobalDataFixup::processRelocations(Module &M, Function &F, RelocationEntries &relocEntries)
{
    ProgramCounters counters;
    getProgramCounters(F, counters);
    assert((counters.size() > 0) && "There are no program counter markers in the function");


    foreach(it, counters.begin(), counters.end()) {
        uint64_t pc = (*it).first;
        std::cout << "Reloc checking pc 0x" << std::hex << pc << std::endl;
        ProgramCounters::iterator lbit = counters.upper_bound(pc);

        uint64_t upper = 0;
        if (lbit == counters.end()) {
            //No upper bound, end of function.
            break;
        }else {
            upper = (*lbit).first;
            assert(pc < upper);
        }

        //Fetch the relocation entries for this pc
        RelocationEntries::iterator lowerIt = relocEntries.lower_bound(pc);
        RelocationEntries::iterator upperIt = relocEntries.upper_bound(upper);


        while(lowerIt != upperIt) {
            uint64_t address = (*lowerIt).first;
            const arelent &relent = (*lowerIt).second;

            //Read the real value from the BFD
            uint32_t data;
            bool b = m_binary->read(address, &data, sizeof(data));
            assert(b && "Could not read from binary file");

            std::cout << "Processing relocation for pc=0x" << std::hex << pc <<
                    "  address=0x" << address <<  "  originalData=0x" << data << std::endl;

            assert(relent.addend == 0 && "Relocation type not supported!");

            //Scan the function to fetch the LLVM constant value
            Instruction *owner;
            ConstantInt *cste = findValue((*it).second, data, &owner);
            assert(cste && "Could not find value to relocate in LLVM stream. Bad.");

            //std::cout << *cste << std::endl;

            //Patch the value
            patchPointer(M, cste, owner, data);

            ++lowerIt;
        }
    }


}

//This can only be run once on the module
bool GlobalDataFixup::runOnModule(llvm::Module &M)
{
    initMemOps(M);
    injectDataSections(M);

    RelocationEntries relocEntries;
    loadRelocations(relocEntries);

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
        processRelocations(M, f, relocEntries);
    }

#if 0
    //For each call site, patch the pointer with the address of
    //the global variable.
    foreach(it, memOpsCalls.begin(), memOpsCalls.end()) {
        CallInst *ci = *it;
        unsigned size = (*m_memops.find(ci->getCalledFunction())).second.first;
        bool isLoad = (*m_memops.find(ci->getCalledFunction())).second.second;
        patchAddress(M, *it, isLoad, size);

    }
#endif
    return false;
}
