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

#include "StaticTranslator.h"
#include "CFG/CBasicBlock.h"
#include "Passes/ConstantExtractor.h"
#include "Passes/JumpTableExtractor.h"
#include "Passes/SystemMemopsRemoval.h"
#include "Passes/GlobalDataFixup.h"
#include "Passes/CallBuilder.h"

#include "Utils.h"

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

///////////////////////////////////////////////////////////////////////////////
//Intercepts code loading functions
///////////////////////////////////////////////////////////////////////////////
extern "C" {
int ldsb_code(target_ulong ptr)
{
    uint8_t val;
    if (!s_currentBinary->read(ptr, &val, sizeof(val))) {
        throw InvalidAddressException(ptr, 1);
    }
    return (int)(int8_t)val;
}

int ldub_code(target_ulong ptr)
{
    uint8_t val;
    if (!s_currentBinary->read(ptr, &val, sizeof(val))) {
        throw InvalidAddressException(ptr, 1);
    }
    return (int)val;
}


int lduw_code(target_ulong ptr)
{
    uint16_t val;
    if (!s_currentBinary->read(ptr, &val, sizeof(val))) {
        throw InvalidAddressException(ptr, 2);
    }
    return val;
}

int ldsw_code(target_ulong ptr)
{
    uint16_t val;
    if (!s_currentBinary->read(ptr, &val, sizeof(val))) {
        throw InvalidAddressException(ptr, 2);
    }
    return (int)(int16_t)val;
}

int ldl_code(target_ulong ptr)
{
    uint32_t val;
    if (!s_currentBinary->read(ptr, &val, sizeof(val))) {
        throw InvalidAddressException(ptr, 4);
    }
    return val;
}

uint64_t ldq_code(target_ulong ptr)
{
    uint64_t val;
    if (!s_currentBinary->read(ptr, &val, sizeof(val))) {
        throw InvalidAddressException(ptr, 8);
    }
    return val;
}
}
///////////////////////////////////////////////////////////////////////////////

bool StaticTranslatorTool::s_translatorInited = false;

StaticTranslatorTool::StaticTranslatorTool()
{
    m_startTime = llvm::sys::TimeValue::now().usec();

    m_binary = new BFDInterface(InputFile, false);
    if (!m_binary->initialize(BfdFormat)) {
        std::cerr << "Could not open " << InputFile << std::endl;
        exit(-1);
    }

    s_currentBinary = m_binary;

    if (!s_translatorInited) {
        cpu_gen_init();
        tcg_llvm_ctx = tcg_llvm_initialize();
        optimize_flags_init();

        //Link in the helper bitcode file
        llvm::sys::Path libraryPath(BitcodeLibrary);
        Linker linker("translator", tcg_llvm_ctx->getModule(), false);
        bool native = false;

        if (linker.LinkInFile(libraryPath, native)) {
            std::cerr <<  "linking in library " << BitcodeLibrary  << " failed!" << std::endl;
            exit(-1);
        }
        linker.releaseModule();

        tcg_llvm_ctx->initializeHelpers();
    }

    std::string translatedFile = OutputDir + "/translated.bin";
    m_translatedCode = new std::ofstream(translatedFile.c_str(), std::ios::binary);

    std::ios::openmode io_mode = std::ios::out | std::ios::trunc
                                  | std::ios::binary;


    std::string messages = OutputDir + "/messages.txt";
    m_messages.open(messages.c_str(), io_mode);

    std::string debug = OutputDir + "/debug.txt";
    m_debug.open(debug.c_str(), io_mode);
}

//XXX: the translator is global...
StaticTranslatorTool::~StaticTranslatorTool()
{
    delete m_translatedCode;

    s_currentBinary = NULL;

    if (m_binary) {
        delete m_binary;
    }
    tcg_llvm_close(tcg_llvm_ctx);
    s_translatorInited = false;
}

void StaticTranslatorTool::translateBlockToX86_64(uint64_t address, void *buffer, int *codeSize)
{
    CPUState env;
    TranslationBlock tb;

    memset(&env, 0, sizeof(env));
    memset(&tb, 0, sizeof(tb));

    QTAILQ_INIT(&env.breakpoints);
    QTAILQ_INIT(&env.watchpoints);

    env.eip = address;
    tb.pc = env.eip;
    tb.cs_base = 0;
    tb.tc_ptr = (uint8_t*)buffer;
    tb.flags = (1 << HF_PE_SHIFT) | (1 << HF_CS32_SHIFT) | (1 << HF_SS32_SHIFT);

    cpu_gen_code(&env, &tb, codeSize);
}

static uint8_t s_dummyBuffer[1024*1024];

CBasicBlock* StaticTranslatorTool::translateBlockToLLVM(uint64_t address)
{
    CPUState env;
    TranslationBlock tb;

    memset(&env, 0, sizeof(env));
    memset(&tb, 0, sizeof(tb));

    QTAILQ_INIT(&env.breakpoints);
    QTAILQ_INIT(&env.watchpoints);

    int codeSize;

    env.eip = address;
    tb.pc = env.eip;
    tb.cs_base = 0;
    tb.tc_ptr = s_dummyBuffer;
    tb.flags = (1 << HF_PE_SHIFT) | (1 << HF_CS32_SHIFT) | (1 << HF_SS32_SHIFT);

    try {
        //Must retranslate twice to get a correct size of tb.
        cpu_gen_code(&env, &tb, &codeSize);
        cpu_gen_llvm(&env, &tb);
    }catch(InvalidAddressException &e) {
        std::cerr << "Could not access address 0x" << std::hex << e.getAddress() << std::endl;
        return NULL;
    }



    /*TB_DEFAULT=0,
    TB_JMP, TB_JMP_IND,
    TB_COND_JMP, TB_COND_JMP_IND,
    TB_CALL, TB_CALL_IND, TB_REP, TB_RET*/

    EBasicBlockType bbType;
    switch(tb.s2e_tb_type) {
        case TB_DEFAULT:      bbType = BB_DEFAULT; break;
        case TB_JMP:          bbType = BB_JMP; break;
        case TB_JMP_IND:      bbType = BB_JMP_IND; break;
        case TB_COND_JMP:     bbType = BB_COND_JMP; break;
        case TB_COND_JMP_IND: bbType = BB_COND_JMP_IND; break;
        case TB_CALL:         bbType = BB_CALL; break;
        case TB_CALL_IND:     bbType = BB_CALL_IND; break;
        case TB_REP:          bbType = BB_REP; break;
        case TB_RET:          bbType = BB_RET; break;
        default: assert(false && "Unsupported translation block type");
    }

    Function *f = (Function*)tb.llvm_function;
//    std::cout << "ORIG:" << *f << std::endl;
    return new CBasicBlock(f, address, tb.size, bbType);
}


void StaticTranslatorTool::translateToX86_64()
{

    uint64_t ep = getEntryPoint();

    if (!ep) {
        std::cerr << "Could not get entry point of " << InputFile << std::endl;
    }

    uint8_t buffer[4096];
    int codeSize = 0;

    translateBlockToX86_64(ep, buffer, &codeSize);

    m_translatedCode->write((const char*)buffer, codeSize);
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
        std::cerr << "Could not split block" << std::endl;
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
                std::cout << "L: Successor of 0x" << std::hex << bb->getAddress() << " is 0x" <<
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

void StaticTranslatorTool::extractAddresses(CBasicBlock *bb)
{
    JumpTableExtractor jumpTableExtractor;
    uint64_t jumpTableAddress = 0;

    if (bb->isIndirectJump()) {
        jumpTableExtractor.runOnFunction(*bb->getFunction());
        jumpTableAddress = jumpTableExtractor.getJumpTableAddress();
        if (jumpTableAddress) {
            std::cout << "Found jump table at 0x" << std::hex << jumpTableAddress << std::endl;
        }
    }

    ConstantExtractor extractor;
    extractor.runOnFunction(*bb->getFunction());

    const ConstantExtractor::Constants &consts = extractor.getConstants();
    foreach(it, consts.begin(), consts.end()) {
        uint64_t addr = *it;
        //Disacard anything that falls outside the binary
        if (!m_binary->isCode(addr)) {
            continue;
        }

        //Skip if the address falls inside the current bb
        if (addr >= bb->getAddress() && (addr < bb->getAddress() + bb->getSize())) {
            continue;
        }

        //Skip jump tables
        if (addr == jumpTableAddress) {
            continue;
        }

        //Skip strings
        std::string str;
        if (checkString(addr, str, false) || checkString(addr, str, true)) {
            std::cout << "Found string at 0x" << std::hex << addr << ": " << str << std::endl;
            continue;
        }

        //Skip if we already explored the basic block
        CBasicBlock cmp(addr, 1);
        if (m_exploredBlocks.find(&cmp) != m_exploredBlocks.end()) {
            continue;
        }

        std::cout << "L: Found new address 0x" << std::hex << addr << std::endl;
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
    Imports imp;
    imp = m_binary->getImports();
    Imports::const_iterator it;
    for (it = imp.begin(); it != imp.end(); ++it) {
        std::pair<std::string, std::string> fcnDesc = (*it).second;
        m_debug << (*it).first << " " << fcnDesc.first << " " << std::hex << fcnDesc.second << std::endl;
    }


    uint64_t ep = getEntryPoint();

    if (!ep) {
        std::cerr << "Could not get entry point of " << InputFile << std::endl;
        exit(-1);
    }

    m_addressesToExplore.insert(ep);
    while(!m_addressesToExplore.empty()) {
        uint64_t ep = *m_addressesToExplore.begin();
        m_addressesToExplore.erase(ep);

        m_debug << "L: Translating at address 0x" << std::hex << ep << std::endl;

        CBasicBlock *bb = translateBlockToLLVM(ep);
        if (!bb) {
            continue;
        }

        extractAddresses(bb);
        bb->toString(std::cout);
        processTranslationBlock(bb);
    }

    m_debug << "There are " << std::dec << m_exploredBlocks.size() << " bbs" << std::endl;
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
            m_debug << std::hex << "0x" << bb->getAddress() << " -> 0x" << (*bbit)->getAddress() << std::endl;
        }
    }

    m_debug << "Blocks with incoming edge: " << std::dec << blocksWithIncomingEdges.size() << std::endl;

    std::set_difference(m_exploredBlocks.begin(), m_exploredBlocks.end(),
                        blocksWithIncomingEdges.begin(), blocksWithIncomingEdges.end(),
                          std::inserter(blocksWithoutIncomingEdges, blocksWithoutIncomingEdges.begin()),
                          BasicBlockComparator());

    m_debug << "Blocks without incoming edge: " << std::dec << blocksWithoutIncomingEdges.size() << std::endl;

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
        m_debug << "FCN: 0x" << std::hex << *it << std::endl;
    }

    m_debug << "There are " << std::dec << m_functionHeaders.size() << " functions" << std::endl;
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

    const char *helperFunctionsStr[] ={"helper_bsf", "helper_bsr", "helper_lzcnt",
                                   "libc__fputs"};

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
    GlobalDataFixup globalData(m_binary);
    globalData.runOnModule(*module);


    //Resolve function calls
    CallBuilder callBuilder(m_functions, m_binary);
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

        m_messages << "processing " << f->getNameStr() << std::endl;

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
    std::string execTraceFile = OutputDir;
    execTraceFile += "/";
    execTraceFile += "module.bc";

    std::ofstream o(execTraceFile.c_str(), std::ofstream::binary);

    llvm::Module *module = tcg_llvm_ctx->getModule();
    module->setTargetTriple("i386-apple-darwin10.4");
    module->setDataLayout("e-p:32:32:32-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:32:64-f32:32:32-f64:32:64-v64:64:64-v128:128:128-a0:0:64-f80:128:128");

    // Output the bitcode file to stdout
    llvm::WriteBitcodeToFile(module, o);
    o.close();
}

void StaticTranslatorTool::dumpStats()
{
    std::string basicBlockListFile = OutputDir + "/bblist.txt";
    std::ofstream bbFile(basicBlockListFile.c_str(), std::ios::binary);

    foreach(it, m_exploredBlocks.begin(), m_exploredBlocks.end()) {
        CBasicBlock *bb = *it;
        bbFile << std::hex << "0x" << bb->getAddress() << std::endl;
    }

    std::string functionListFile = OutputDir + "/functions.txt";
    std::ofstream fcnFile(functionListFile.c_str(), std::ios::binary);

    foreach(it, m_functions.begin(), m_functions.end()) {
        CFunction *fcn = *it;
        fcnFile << std::hex << "0x" << fcn->getAddress() << std::endl;
    }

    m_endTime = llvm::sys::TimeValue::now().usec();
    std::string statsFileStr = OutputDir + "/stats.txt";
    std::ofstream statsFile(statsFileStr.c_str(), std::ios::binary);

    statsFile << "Execution time: " << std::dec << (m_endTime - m_startTime) / 1000000.0 << std::endl;
    statsFile << "Basic blocks:   " << m_exploredBlocks.size() << std::endl;
    statsFile << "Functions:      " << m_functions.size() << std::endl;
}

}
}

void initializeOutputDirectory()
{
    llvm::sys::Path cwd(OutputDir.getValue());

    for (int i = 0; ; i++) {
        std::ostringstream dirName;
        dirName << "x2l-out-" << i;

        llvm::sys::Path dirPath(cwd);
        dirPath.appendComponent(dirName.str());

        if(!dirPath.exists()) {
            OutputDir = dirPath.toString();
            break;
        }
    }


    std::cout << "X2L: output directory = \"" << OutputDir << "\"\n";

#ifdef _WIN32
    if(mkdir(OutputDir.c_str()) < 0) {
#else
    if(mkdir(OutputDir.c_str(), 0775) < 0)
#endif
    {
        perror("ERROR: Unable to create output directory");
        exit(1);
    }


#ifndef _WIN32
    llvm::sys::Path s2eLast(".");
    s2eLast.appendComponent("x2l-last");

    if ((unlink(s2eLast.c_str()) < 0) && (errno != ENOENT)) {
        perror("ERRPR: Cannot unlink x2l-last");
        exit(1);
    }

    if (symlink(OutputDir.getValue().c_str(), s2eLast.c_str()) < 0) {
        perror("ERROR: Cannot make symlink x2l-last");
        exit(1);
    }
#endif
}

int main(int argc, char** argv)
{
    cl::ParseCommandLineOptions(argc, (char**) argv);

    if (ExpMode) {
        initializeOutputDirectory();
    }

    StaticTranslatorTool translator;

    translator.exploreBasicBlocks();
    translator.extractFunctions();
    translator.reconstructFunctions();

    if (translator.getFunctions().size() == 0) {
        std::cout << "No functions found" << std::endl;
        return -1;
    }

    translator.cleanupCode();
    translator.renameEntryPoint();
    translator.outputBitcodeFile();

    translator.dumpStats();

    return 0;
}
