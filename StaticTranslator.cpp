extern "C" {
#include <qemu-common.h>
#include <cpu-all.h>
#include <exec-all.h>
#include <tcg/tcg.h>
#include <tcg/tcg-llvm.h>
}

#include "llvm/Support/CommandLine.h"
#include "llvm/Function.h"
#include "llvm/Linker.h"
#include <stdio.h>
#include <inttypes.h>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>

#include <lib/BinaryReaders/BFDInterface.h>

#include "StaticTranslator.h"
#include "CFG/CBasicBlock.h"

using namespace llvm;
using namespace s2etools;
using namespace s2etools::translator;
using namespace s2e::plugins;


namespace {
cl::opt<std::string>
    InputFile(cl::Positional, cl::Required, cl::desc("<input file>"));

cl::opt<std::string>
    BitcodeLibrary("bitcodelibrary", cl::Required, cl::desc("Translator bitcode file"));

cl::opt<std::string>
    OutputDir("outputdir", cl::desc("Store the analysis output in this directory"), cl::init("."));
}

namespace s2etools
{
static BFDInterface *s_currentBinary = NULL;

///////////////////////////////////////////////////////////////////////////////
//Intercepts code loading functions
///////////////////////////////////////////////////////////////////////////////
extern "C" {
int ldsb_code(target_ulong ptr)
{
    uint8_t val;
    if (!s_currentBinary->read(ptr, &val, sizeof(val))) {
        throw InvalidAddressException();
    }
    return (int)(int8_t)val;
}

int ldub_code(target_ulong ptr)
{
    uint8_t val;
    if (!s_currentBinary->read(ptr, &val, sizeof(val))) {
        throw InvalidAddressException();
    }
    return (int)val;
}


int lduw_code(target_ulong ptr)
{
    uint16_t val;
    if (!s_currentBinary->read(ptr, &val, sizeof(val))) {
        throw InvalidAddressException();
    }
    return val;
}

int ldsw_code(target_ulong ptr)
{
    uint16_t val;
    if (!s_currentBinary->read(ptr, &val, sizeof(val))) {
        throw InvalidAddressException();
    }
    return (int)(int16_t)val;
}

int ldl_code(target_ulong ptr)
{
    uint32_t val;
    if (!s_currentBinary->read(ptr, &val, sizeof(val))) {
        throw InvalidAddressException();
    }
    return val;
}

uint64_t ldq_code(target_ulong ptr)
{
    uint64_t val;
    if (!s_currentBinary->read(ptr, &val, sizeof(val))) {
        throw InvalidAddressException();
    }
    return val;
}
}
///////////////////////////////////////////////////////////////////////////////

bool StaticTranslatorTool::s_translatorInited = false;

StaticTranslatorTool::StaticTranslatorTool()
{
    m_binary = new BFDInterface(InputFile, false);
    if (!m_binary->initialize()) {
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

CBasicBlock* StaticTranslatorTool::translateBlockToLLVM(uint64_t address)
{
    CPUState env;
    TranslationBlock tb;

    memset(&env, 0, sizeof(env));
    memset(&tb, 0, sizeof(tb));

    QTAILQ_INIT(&env.breakpoints);
    QTAILQ_INIT(&env.watchpoints);

    uint8_t dummyBuffer[4096];
    int codeSize;

    env.eip = address;
    tb.pc = env.eip;
    tb.cs_base = 0;
    tb.tc_ptr = dummyBuffer;
    tb.flags = (1 << HF_PE_SHIFT) | (1 << HF_CS32_SHIFT) | (1 << HF_SS32_SHIFT);

    cpu_gen_code(&env, &tb, &codeSize);
    cpu_gen_llvm(&env, &tb);

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

    return new CBasicBlock((Function*)tb.llvm_function, address, codeSize, bbType);
}


void StaticTranslatorTool::translateToX86_64()
{

    uint64_t ep = m_binary->getEntryPoint();
    if (!ep) {
        std::cerr << "Could not get entry point of " << InputFile << std::endl;
    }

    uint8_t buffer[4096];
    int codeSize = 0;

    translateBlockToX86_64(ep, buffer, &codeSize);

    m_translatedCode->write((const char*)buffer, codeSize);
}

void StaticTranslatorTool::translateToLLVM()
{

    uint64_t ep = m_binary->getEntryPoint();
    if (!ep) {
        std::cerr << "Could not get entry point of " << InputFile << std::endl;
    }


    const BFDInterface::Imports &imp = m_binary->getImports();
    BFDInterface::Imports::const_iterator it;
    for (it = imp.begin(); it != imp.end(); ++it) {
        const BFDInterface::FunctionDescriptor &fcnDesc = (*it).second;
        std::cout << (*it).first << " " << fcnDesc.first << " " << std::hex << fcnDesc.second << std::endl;
    }

    CBasicBlock *bb = translateBlockToLLVM(ep);
    std::cout << *bb->getFunction();
}


}


int main(int argc, char** argv)
{
    cl::ParseCommandLineOptions(argc, (char**) argv);
    StaticTranslatorTool translator;

    translator.translateToLLVM();
    return 0;
}
