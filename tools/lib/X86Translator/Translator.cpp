extern "C" {
#include <qemu-common.h>
#include <cpu-all.h>
#include <exec-all.h>
#include <tcg/tcg.h>
#include <tcg/tcg-llvm.h>
}

#include <llvm/Linker.h>
#include <sstream>

#include "Translator.h"

namespace s2etools {

Binary *s_currentBinary = NULL;
bool Translator::s_translatorInited = false;

/*****************************************************************************
 * The following functions are invoked by the QEMU translator to read code.
 * This library redirects them to the binary file
 *****************************************************************************/
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

/*****************************************************************************/

using namespace llvm;

Translator::Translator(const llvm::sys::Path &bitcodeLibrary) {
    m_binary = NULL;

    if (!s_translatorInited) {
        //Link in the helper bitcode file
        Linker linker("translator", tcg_llvm_ctx->getModule(), false);
        bool native = false;

        if (linker.LinkInFile(bitcodeLibrary, native)) {
            std::stringstream ss;
            ss << "Linking in library " << bitcodeLibrary.c_str()  << " failed!";
            throw TranslatorException(ss.str());
            return;
        }

        linker.releaseModule();

        cpu_gen_init();
        tcg_llvm_ctx = tcg_llvm_initialize();
        optimize_flags_init();
        tcg_llvm_ctx->initializeHelpers();
    }
}

Translator::~Translator()
{
    if (s_translatorInited) {
        tcg_llvm_close(tcg_llvm_ctx);
        s_translatorInited = false;
    }
}

void Translator::setBinaryFile(Binary *binary)
{
    m_binary = binary;
    s_currentBinary = binary;
}

/*****************************************************************************/

X86Translator::X86Translator(const llvm::sys::Path &bitcodeLibrary):Translator(bitcodeLibrary)
{

}

X86Translator::~X86Translator()
{

}

LLVMBasicBlock X86Translator::translate(uint64_t address)
{
    static uint8_t s_dummyBuffer[1024*1024];
    CPUState env;
    TranslationBlock tb;
    LLVMBasicBlock ret;

    if (!isInitialized()) {
        throw TranslatorNotInitializedException();
    }

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

    //Must retranslate twice to get a correct size of tb.
    //May throw InvalidAddressException
    cpu_gen_code(&env, &tb, &codeSize);
    cpu_gen_llvm(&env, &tb);

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

    ret.address = address;
    ret.size = tb.size;
    ret.type = bbType;
    ret.function = (Function*)tb.llvm_function;

    return ret;
}

}
