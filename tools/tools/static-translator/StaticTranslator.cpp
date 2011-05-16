extern "C" {
#include <qemu-common.h>
#include <cpu-all.h>
#include <exec-all.h>
#include <tcg/tcg.h>
#include <tcg/tcg-llvm.h>
}

#include <llvm/Bitcode/ReaderWriter.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Function.h>
#include <llvm/Module.h>
#include <llvm/Linker.h>

#include <llvm/System/Path.h>

#include <llvm/Transforms/Scalar.h>
#include <llvm/ModuleProvider.h>
#include <llvm/PassManager.h>
#include <llvm/Analysis/Verifier.h>
#include <llvm/Analysis/Passes.h>
#include <llvm/target/TargetData.h>

#include <stdio.h>
#include <inttypes.h>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <sys/stat.h>


#include <lib/BinaryReaders/BFDInterface.h>
#include <lib/Utils/Log.h>

#include "StaticTranslator.h"
#include "CFG/CBasicBlock.h"
#include "Passes/ConstantExtractor.h"
#include "Passes/JumpTableExtractor.h"
#include "Passes/SystemMemopsRemoval.h"
#include "Passes/GlobalDataFixup.h"
#include "Passes/CallBuilder.h"

#include "lib/Utils/Utils.h"

using namespace llvm;
using namespace s2etools;
using namespace s2etools::translator;
using namespace s2e::plugins;

struct MyQwordParser : public cl::basic_parser<uint64_t> {
  // parse - Return true on error.
  bool parse(cl::Option &O, const char *ArgName, const std::string &Arg,
             uint64_t &Val) {
      const char *ArgStart = Arg.c_str();
      char *End;
      Val = strtol(ArgStart, &End, 0);
//      std::cout << "ArgStart=" << ArgStart << " val=" << std::hex << Val << std::endl;
      return false;
  }
};



namespace {
cl::opt<std::string>
    InputFile(cl::Positional, cl::Required, cl::desc("<input file>"));

cl::opt<std::string>
    BitcodeLibrary("bitcodelibrary", cl::Required, cl::desc("Translator bitcode file"));

cl::opt<std::string>
    OutputDir("outputdir", cl::desc("Store the analysis output in this directory"), cl::init("."));

cl::opt<bool>
    ExpMode("expmode", cl::desc("Auto-increment the x2e-out-* folder and create x2l-last symlink"), cl::init(false));


cl::opt<std::string>
    BfdFormat("bfd", cl::desc("Binary format of the input (in case auto detection fails)"), cl::init(""));

cl::opt<uint64_t, false, MyQwordParser >
    EntryPointAddress("entrypoint", cl::desc("<address> Override the address of the default entry point"), cl::init(0));
}

namespace s2etools {
namespace translator {
static BFDInterface *s_currentBinary = NULL;
std::string StaticTranslatorTool::TAG = "StaticTranslatorTool";

bool StaticTranslatorTool::s_translatorInited = false;

StaticTranslatorTool::StaticTranslatorTool()
{
    if (ExpMode) {
        m_experiment = new ExperimentManager(OutputDir, "x2l");
    }else {
        m_experiment = new ExperimentManager(OutputDir);
    }

    m_startTime = llvm::sys::TimeValue::now().usec();

    m_bfd = new BFDInterface(InputFile, false);
    if (!m_bfd->initialize(BfdFormat)) {
        LOGDEBUG() << "Could not open " << InputFile << std::endl;
        exit(-1);
    }

    m_binary = m_bfd->getBinary();

    llvm::sys::Path libraryPath(BitcodeLibrary);
    m_translator = new X86Translator(libraryPath);
    m_translator->setBinaryFile(m_binary);
    m_translator->setSingleStep(true);
    m_translatedCode = m_experiment->getOuputFile("translated.bin");
}

//XXX: the translator is global...
StaticTranslatorTool::~StaticTranslatorTool()
{
    delete m_translatedCode;
    delete m_experiment;

    s_currentBinary = NULL;

    if (m_bfd) {
        delete m_bfd;
    }
    m_binary = NULL;

    delete m_translator;

}


void StaticTranslatorTool::translateAllInstructions()
{
    uint64_t ep = getEntryPoint();

    if (!ep) {
        LOGERROR() << "Could not get entry point of " << InputFile << std::endl;
        exit(-1);
    }

    m_addressesToExplore.insert(ep);
    while(!m_addressesToExplore.empty()) {
        uint64_t addr = *m_addressesToExplore.begin();
        m_addressesToExplore.erase(addr);

        if (m_exploredAddresses.find(addr) != m_exploredAddresses.end()) {
            continue;
        }
        m_exploredAddresses.insert(addr);

        LOGDEBUG() << "L: Translating at address 0x" << std::hex << addr << std::endl;

        TranslatedBlock bblock;
        try {
            bblock = m_translator->translate(addr);
        }catch(InvalidAddressException &e) {
            LOGDEBUG() << "Could not access address 0x" << std::hex << e.getAddress() << std::endl;
            continue;
        }
        extractAddresses(bblock.getFunction(), bblock.isIndirectJump());

    }

    LOGINFO() << "There are " << std::dec << m_translatedInstructions.size() << " instructions" << std::endl;

}


void StaticTranslatorTool::splitExistingBlock(CBasicBlock *newBlock, CBasicBlock *existingBlock)
{
    //The new block overlaps with another one.
    //Decide how to split.
    CBasicBlock *blockToDelete = NULL, *blockToSplit = NULL;
    uint64_t splitAddress = 0;


    if (newBlock->getAddress() < existingBlock->getAddress()) {
        //The new block is bigger. Split it and remove the
        //existing one.
        blockToDelete = existingBlock;
        blockToSplit = newBlock;
        splitAddress = existingBlock->getAddress();

        //Check if the bigger block has the same instructions as the smaller one.
        //It may happen that disassembling at slightly different offsets can cause problems
        //(e.g., disassembling a string might disassemble some valid code past the string).

        //XXX: Broken. We might as well get a new block that is better, in which case we should discard
        //the previous one. This requires heuristics...
        //CBasicBlock::AddressSet commonAddresses;
        //blockToSplit->intersect(existingBlock, commonAddresses);


    }else if (newBlock->getAddress()>existingBlock->getAddress()) {
        //Discard the new block, and split the exising one
        blockToDelete = newBlock;
        blockToSplit = existingBlock;
        splitAddress = newBlock->getAddress();
    }else {
        //The new block is equal to a previous one
        return;
    }


    CBasicBlock *split = blockToSplit->split(splitAddress);

    if (!split) {
        LOGDEBUG() << "Could not split block" << std::endl;
        return;
    }

    m_exploredBlocks.erase(blockToSplit);
    delete blockToDelete;

    m_exploredBlocks.insert(blockToSplit);
    m_exploredBlocks.insert(split);
}

void StaticTranslatorTool::processTranslationBlock(CBasicBlock *bb)
{
    BasicBlocks::iterator bbit = m_exploredBlocks.find(bb);
    if (bbit == m_exploredBlocks.end()) {
        m_exploredBlocks.insert(bb);
        //Check that successors have not been explored yet
        const CBasicBlock::Successors &suc = bb->getSuccessors();
        foreach(sit, suc.begin(), suc.end()) {
            if (m_addressesToExplore.find(*sit) == m_addressesToExplore.end()) {
                LOGDEBUG() << "L: Successor of 0x" << std::hex << bb->getAddress() << " is 0x" <<
                        *sit << std::endl;
                m_addressesToExplore.insert(*sit);
            }
        }
    } else {
        splitExistingBlock(bb, *bbit);
    }
}

bool StaticTranslatorTool::checkString(uint64_t address, std::string &res, bool isUnicode)
{
    std::string ret;
    unsigned char c;
    uint16_t u;

    do {
        if (isUnicode) {
            if (!m_binary->read(address, &u, sizeof(u))) {
                return false;
            }
        }else {
            if (!m_binary->read(address, &c, sizeof(c))) {
                return false;
            }
            u = c;
        }

        if (u > 0 && (u < 0x20 || u >= 0x80) && u != 0xd && u != 0xa) {
            return false;
        }
        if (u) {
            ret = ret + (char)u;
        }

        address = isUnicode ? address + 2 : address + 1;
    }while(u);

    //XXX: Improve the heuristic
    if (ret.size() < 2) {
        return false;
    }

    res = ret;
    return true;
}

void StaticTranslatorTool::extractAddresses(llvm::Function *llvmInstruction, bool isIndirectJump)
{
    JumpTableExtractor jumpTableExtractor;
    uint64_t jumpTableAddress = 0;

    if (isIndirectJump) {
        jumpTableExtractor.runOnFunction(*llvmInstruction);
        jumpTableAddress = jumpTableExtractor.getJumpTableAddress();
        if (jumpTableAddress) {
            LOGDEBUG() << "Found jump table at 0x" << std::hex << jumpTableAddress << std::endl;
        }
    }

    ConstantExtractor extractor;
    extractor.runOnFunction(*llvmInstruction);

    const ConstantExtractor::Constants &consts = extractor.getConstants();
    foreach(it, consts.begin(), consts.end()) {
        uint64_t addr = *it;
        //Disacard anything that falls outside the binary
        if (!m_bfd->isCode(addr)) {
            continue;
        }

#if 0
        //Skip if the address falls inside the current bb
        if (addr >= bb->getAddress() && (addr < bb->getAddress() + bb->getSize())) {
            continue;
        }
#endif

        //Skip jump tables
        if (addr == jumpTableAddress) {
            continue;
        }

        //Skip strings
        std::string str;
        if (checkString(addr, str, false) || checkString(addr, str, true)) {
            LOGDEBUG() << "Found string at 0x" << std::hex << addr << ": " << str << std::endl;
            continue;
        }

        //Skip if we already explored the basic block
        CBasicBlock cmp(addr, 1);
        if (m_exploredBlocks.find(&cmp) != m_exploredBlocks.end()) {
            continue;
        }

        LOGDEBUG() << "L: Found new address 0x" << std::hex << addr << std::endl;
        m_addressesToExplore.insert(addr);
    }
}

uint64_t StaticTranslatorTool::getEntryPoint()
{
    if (EntryPointAddress) {
        return EntryPointAddress;
    }else {
        return m_binary->getEntryPoint();
    }
}

void StaticTranslatorTool::exploreBasicBlocks()
{
#if 0
    Imports imp;
    imp = m_binary->getImports();
    Imports::const_iterator it;
    for (it = imp.begin(); it != imp.end(); ++it) {
        std::pair<std::string, std::string> fcnDesc = (*it).second;
        LOGDEBUG() << (*it).first << " " << fcnDesc.first << " " << std::hex << fcnDesc.second << std::endl;
    }


    uint64_t ep = getEntryPoint();

    if (!ep) {
        LOGERROR() << "Could not get entry point of " << InputFile << std::endl;
        exit(-1);
    }

    m_addressesToExplore.insert(ep);
    while(!m_addressesToExplore.empty()) {
        uint64_t ep = *m_addressesToExplore.begin();
        m_addressesToExplore.erase(ep);

        LOGDEBUG() << "L: Translating at address 0x" << std::hex << ep << std::endl;

        CBasicBlock *bb = translateBlockToLLVM(ep);
        if (!bb) {
            continue;
        }

        extractAddresses(bb);
        processTranslationBlock(bb);
    }

    LOGINFO() << "There are " << std::dec << m_exploredBlocks.size() << " bbs" << std::endl;
#endif
}

void StaticTranslatorTool::extractFunctions()
{
    typedef std::map<CBasicBlock*, BasicBlocks> Graph;

    Graph incomingEdges, outgoingEdges;

    BasicBlocks blocksWithIncomingEdges;
    BasicBlocks blocksWithoutIncomingEdges;

    foreach(it, m_exploredBlocks.begin(), m_exploredBlocks.end()) {
        CBasicBlock *bb = *it;

        //Check the case of function stubs that have only one indirect branch
        if (bb->isIndirectJump() && bb->getInstructionCount() == 1) {
            //XXX: check that somebody actually calls it
            continue;
        }

        const CBasicBlock::Successors &suc = bb->getSuccessors();
        foreach(sucit, suc.begin(), suc.end()) {
            //Look for the basic block descriptor given its address
            CBasicBlock tofind(*sucit, 1);
            BasicBlocks::iterator bbit = m_exploredBlocks.find(&tofind);
            if (bbit == m_exploredBlocks.end()) {
                continue;
            }

            blocksWithIncomingEdges.insert(*bbit);
            LOGDEBUG() << std::hex << "0x" << bb->getAddress() << " -> 0x" << (*bbit)->getAddress() << std::endl;
        }
    }

    LOGINFO() << "Blocks with incoming edge: " << std::dec << blocksWithIncomingEdges.size() << std::endl;

    std::set_difference(m_exploredBlocks.begin(), m_exploredBlocks.end(),
                        blocksWithIncomingEdges.begin(), blocksWithIncomingEdges.end(),
                          std::inserter(blocksWithoutIncomingEdges, blocksWithoutIncomingEdges.begin()),
                          BasicBlockComparator());

    LOGINFO() << "Blocks without incoming edge: " << std::dec << blocksWithoutIncomingEdges.size() << std::endl;

    //Look for nodes that have no incoming edges.
    //These should be function entry points

    foreach(it, blocksWithoutIncomingEdges.begin(), blocksWithoutIncomingEdges.end()) {
            m_functionHeaders.insert((*it));
    }

    std::set<uint64_t> fcnStartSet;
    foreach(it, m_functionHeaders.begin(), m_functionHeaders.end()) {
        assert(fcnStartSet.find((*it)->getAddress()) == fcnStartSet.end());
        fcnStartSet.insert((*it)->getAddress());
    }

    foreach(it, fcnStartSet.begin(), fcnStartSet.end()) {
        LOGDEBUG() << "FCN: 0x" << std::hex << *it << std::endl;
    }

    LOGINFO() << "There are " << std::dec << m_functionHeaders.size() << " functions" << std::endl;
}

void StaticTranslatorTool::reconstructFunctions()
{
    FunctionAddressMap addrMap;
    foreach(it, m_exploredBlocks.begin(), m_exploredBlocks.end()) {
        //This is to put calls the real basic blocks.
        (*it)->patchCallMarkersWithRealFunctions(m_exploredBlocks);
        addrMap[(*it)->getFunction()] = (*it)->getAddress();
    }


    foreach(it, m_functionHeaders.begin(), m_functionHeaders.end()) {
        CFunction *fcn = new CFunction(*it);
        fcn->generate(addrMap);
        std::cout << fcn->getFunction();
        m_functions.insert(fcn);
        //break;
    }
}

void StaticTranslatorTool::renameEntryPoint()
{
    uint64_t ep = getEntryPoint();
    assert(ep);

    foreach(it, m_functions.begin(), m_functions.end()) {
        CFunction *fcn = *it;
        if (fcn->getAddress() == ep) {
            fcn->getFunction()->setName("__main");
            return;
        }
    }
}

void StaticTranslatorTool::cleanupCode()
{
    Module *module = tcg_llvm_ctx->getModule();
    std::set<Function *> usedFunctions;
    std::set<Function *> builtinFunctions;

    const char *builtinFunctionsStr[] = {"strcmp", "strdup", "strtok",
                                     "strchr", "strtoul", "strlen", "pstrcpy", "snprintf",
                                     "fprintf", "fwrite",
                                     "instruction_marker", "call_marker", "return_marker",
                                     "__ldq_mmu", "__ldl_mmu", "__ldw_mmu", "__ldb_mmu",
                                     "__stq_mmu", "__stl_mmu", "__stw_mmu", "__stb_mmu",
                                     };

    //The idea is to delete the body of unused functions so that there are no linking problems
    //(some of the functions call undefined routines).
    //XXX: This is really ugly. Maybe look at how LLVM does that (link time optimization...)
    const char *helperFunctionsStr[] ={"helper_bsf", "helper_bsr", "helper_lzcnt",
                                   "libc__fputs", "libc__pthread_join", "libc__pthread_create"};

    std::set<Function *> libcFunctions;
    for (unsigned i=0; i<sizeof(builtinFunctionsStr)/sizeof(builtinFunctionsStr[0]); ++i) {
        Function *fcn = module->getFunction(builtinFunctionsStr[i]);
        assert(fcn);
        builtinFunctions.insert(fcn);
    }

    std::set<Function *> helperFunctions;
    for (unsigned i=0; i<sizeof(helperFunctionsStr)/sizeof(helperFunctionsStr[0]); ++i) {
        Function *fcn = module->getFunction(helperFunctionsStr[i]);
        assert(fcn);
        helperFunctions.insert(fcn);
    }


    FunctionPassManager FcnPassManager(new ExistingModuleProvider(module));
    FcnPassManager.add(new SystemMemopsRemoval());
//    FcnPassManager.add(createDeadInstEliminationPass());
//    FcnPassManager.add(createDeadStoreEliminationPass());

    foreach(fcnit, m_functions.begin(), m_functions.end()) {
        usedFunctions.insert((*fcnit)->getFunction());

        //Cleanup the function
        FcnPassManager.run(*(*fcnit)->getFunction());

    }

    //Fix global variable accesses.
    //This must be done __AFTER__ SystemMemopsRemoval because
    //global data fixup replaces ld*_mmu ops with ld/st.
    GlobalDataFixup globalData(m_bfd);
    globalData.runOnModule(*module);


    //Resolve function calls
    CallBuilder callBuilder(m_functions, m_bfd);
    callBuilder.runOnModule(*module);

    //Drop all the useless functions
    foreach(fcnit, module->begin(), module->end()) {
        Function *f = &*fcnit;
        if (f->isIntrinsic() || f->isDeclaration()) {
            continue;
        }

        if (usedFunctions.find(f) != usedFunctions.end()) {
            continue;
        }

        if (helperFunctions.find(f) != helperFunctions.end()) {
            continue;
        }

        LOGDEBUG() << "processing " << f->getNameStr() << std::endl;

        f->deleteBody();

        if (builtinFunctions.find(f) != builtinFunctions.end()) {
            continue;
        }

        //replace with empty bodies
        BasicBlock *bb = BasicBlock::Create(module->getContext(),"", f, NULL);
        if (f->getReturnType() == Type::getInt32Ty(module->getContext())) {
            Value *retVal = ConstantInt::get(module->getContext(), APInt(32,  0));
            ReturnInst::Create(module->getContext(),retVal, bb);
        }else if (f->getReturnType() == Type::getInt64Ty(module->getContext())) {
            Value *retVal = ConstantInt::get(module->getContext(), APInt(64,  0));
            ReturnInst::Create(module->getContext(),retVal, bb);
        }else if (f->getReturnType() == Type::getDoubleTy(module->getContext())) {
            Value *retVal = ConstantFP::get(module->getContext(), APFloat(0.0));
            ReturnInst::Create(module->getContext(),retVal, bb);
        }else if (f->getReturnType() == Type::getFloatTy(module->getContext())) {
            Value *retVal = ConstantFP::get(module->getContext(), APFloat(0.0f));
            ReturnInst::Create(module->getContext(),retVal, bb);
        }else if (f->getReturnType() == Type::getVoidTy(module->getContext())) {
            ReturnInst::Create(module->getContext(),bb);
        }else{
            f->deleteBody();
        }

    }


    PassManager FcnPasses;
    //FcnPasses.add(TD);

    FcnPasses.add(createVerifierPass());
    FcnPasses.add(createCFGSimplificationPass());
    FcnPasses.add(createDeadCodeEliminationPass());
    FcnPasses.add(createInstructionCombiningPass());
    FcnPasses.add(createDeadStoreEliminationPass());
    FcnPasses.add(createGVNPass());
    FcnPasses.add(createAggressiveDCEPass());
    FcnPasses.run(*tcg_llvm_ctx->getModule());
}

void StaticTranslatorTool::outputBitcodeFile()
{

    std::ostream *o = m_experiment->getOuputFile("module.bc");

    llvm::Module *module = tcg_llvm_ctx->getModule();
    module->setTargetTriple("i386-apple-darwin10.4");
    module->setDataLayout("e-p:32:32:32-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:32:64-f32:32:32-f64:32:64-v64:64:64-v128:128:128-a0:0:64-f80:128:128");

    // Output the bitcode file to stdout
    llvm::WriteBitcodeToFile(module, *o);
    delete o;
}

void StaticTranslatorTool::dumpStats()
{
    std::ostream *bbFile = m_experiment->getOuputFile("bblist.txt");

    foreach(it, m_exploredBlocks.begin(), m_exploredBlocks.end()) {
        CBasicBlock *bb = *it;
        *bbFile << std::hex << "0x" << bb->getAddress() << std::endl;
    }

    std::ostream *fcnFile = m_experiment->getOuputFile("functions.txt");

    foreach(it, m_functions.begin(), m_functions.end()) {
        CFunction *fcn = *it;
        *fcnFile << std::hex << "0x" << fcn->getAddress() << std::endl;
    }

    m_endTime = llvm::sys::TimeValue::now().usec();
    std::ostream *statsFile = m_experiment->getOuputFile("stats.txt");

    *statsFile << "Execution time: " << std::dec << (m_endTime - m_startTime) / 1000000.0 << std::endl;
    *statsFile << "Basic blocks:   " << m_exploredBlocks.size() << std::endl;
    *statsFile << "Functions:      " << m_functions.size() << std::endl;

    delete statsFile;
    delete fcnFile;
    delete bbFile;
}

}
}


int main(int argc, char** argv)
{
    cl::ParseCommandLineOptions(argc, (char**) argv);

    StaticTranslatorTool translator;

    translator.translateAllInstructions();

#if 0
    translator.exploreBasicBlocks();
    translator.extractFunctions();
    translator.reconstructFunctions();

    if (translator.getFunctions().size() == 0) {
        std::cout << "No functions found" << std::endl;
        return -1;
    }

    translator.cleanupCode();
    translator.renameEntryPoint();
#endif
    translator.outputBitcodeFile();

    translator.dumpStats();

    return 0;
}
