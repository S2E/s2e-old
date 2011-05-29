extern "C" {
#include <qemu-common.h>
#include <cpu-all.h>
#include <exec-all.h>
#include <tcg/tcg.h>
#include <tcg/tcg-llvm.h>
}

#include <llvm/Linker.h>
#include <llvm/Analysis/Verifier.h>
#include <llvm/Transforms/Scalar.h>
#include <sstream>

#include "Translator.h"
#include "TbPreprocessor.h"
#include "CpuStatePatcher.h"


#include "lib/Utils/Log.h"
#include "lib/Utils/Utils.h"


using namespace llvm;
namespace s2etools {


Binary *s_currentBinary = NULL;
bool Translator::s_translatorInited = false;
LogKey TranslatedBlock::TAG = LogKey("TranslatedBlock");
LogKey Translator::TAG = LogKey("Translator");
LogKey X86Translator::TAG = LogKey("X86Translator");

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
void TranslatedBlock::print(std::ostream &os) const
{
    os << "TB at 0x" << std::hex << m_address << " type=0x" << m_type <<
            " size=0x" << m_size << std::endl;

    unsigned i=0;
    foreach(it, m_successors.begin(), m_successors.end()) {
        Value *v = *it;
        if (!v) {
            ++i;
            continue;
        }

        if (ConstantInt *ci = dyn_cast<ConstantInt>(v)) {
            uint64_t val = ci->getZExtValue();
            os << "succ[" << i << "]=0x" << val << std::endl;
        }else {
            os << "succ[" << i << "]=" << *v << std::endl;
        }
        ++i;
    }
    os << *m_function << std::endl;
}

/*****************************************************************************/

using namespace llvm;

Translator::Translator(const llvm::sys::Path &bitcodeLibrary) {
    m_binary = NULL;
    m_singlestep = false;

    if (!s_translatorInited) {
        cpu_gen_init();
        tcg_llvm_ctx = tcg_llvm_initialize();


        //Link in the helper bitcode file
        Linker linker("translator", tcg_llvm_ctx->getModule(), false);
        bool native = false;

        if (linker.LinkInFile(bitcodeLibrary, native)) {
            LOGERROR("Linking in library " << bitcodeLibrary.c_str()  << " failed!" << std::endl);
            return;
        }

        LOGINFO("Linked in library " << bitcodeLibrary.c_str()  << std::endl);

        optimize_flags_init();
        tcg_llvm_ctx->initializeHelpers();        
        linker.releaseModule();
        s_translatorInited = true;
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

llvm::Module *Translator::getModule() const
{
    return tcg_llvm_ctx->getModule();
}

llvm::ModuleProvider *Translator::getModuleProvider() const
{
    return tcg_llvm_ctx->getModuleProvider();
}

/*****************************************************************************/

X86Translator::X86Translator(const llvm::sys::Path &bitcodeLibrary):Translator(bitcodeLibrary)
{
    m_functionPasses = new FunctionPassManager(tcg_llvm_ctx->getModuleProvider());
    m_functionPasses->add(createCFGSimplificationPass());

    //We need this passes to simplify the translation of the instruction.
    //The code is quite bulky, the fewer instructions, the better.
    m_functionOptPasses = new FunctionPassManager(tcg_llvm_ctx->getModuleProvider());
    //m_functionOptPasses->add(createVerifierPass());
    m_functionOptPasses->add(createDeadCodeEliminationPass());
    m_functionOptPasses->add(createGVNPREPass());
}

X86Translator::~X86Translator()
{
    delete m_functionPasses;
    delete m_functionOptPasses;
}

TranslatedBlock *X86Translator::translate(uint64_t address)
{
    static uint8_t s_dummyBuffer[1024*1024];
    CPUState env;
    TranslationBlock tb;

    if (!isInitialized()) {
        throw TranslatorNotInitializedException();
    }

    memset(&env, 0, sizeof(env));
    memset(&tb, 0, sizeof(tb));

    QTAILQ_INIT(&env.breakpoints);
    QTAILQ_INIT(&env.watchpoints);

    int codeSize;

    //We translate only one instruction at a time.
    //It is much easier to rebuild basic blocks this way.
    env.singlestep_enabled = isSingleStep();

    env.eip = address;
    tb.pc = env.eip;
    tb.cs_base = 0;
    tb.tc_ptr = s_dummyBuffer;
    tb.flags = (1 << HF_PE_SHIFT) | (1 << HF_CS32_SHIFT) | (1 << HF_SS32_SHIFT);

    //Must retranslate twice to get a correct size of tb.
    //May throw InvalidAddressException
    cpu_gen_code(&env, &tb, &codeSize);
    cpu_gen_llvm(&env, &tb);

    //verifyFunction(*(Function*)tb.llvm_function);

    ETranslatedBlockType bbType;
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


    TranslatedBlock *ret = new TranslatedBlock(address, tb.size, (Function*)tb.llvm_function, bbType);

    if (isSingleStep()) {
        Function *fcn = ret->getFunction();

        //LOGDEBUG() << "BEFORE DEUG" <<  std::endl;
        //LOGDEBUG() << *fcn << std::flush;


        //Deuglygepify
        CpuStatePatcher patcher(address);
        patcher.runOnFunction(*fcn);
        fcn = patcher.getTransformed();

        ret->setFunction(fcn);

        //CFG simiplification is required because TbPreprocessor assumes that the
        //program counter is always updated in blocks ending with a ret.
        m_functionPasses->run(*ret->getFunction());

        //LOGDEBUG() << "BEFORE PREP" <<  std::endl;
        //LOGDEBUG() << *fcn << std::flush;

        //Insert various markers
        TbPreprocessor prep(ret);
        prep.runOnFunction(*ret->getFunction());

        //Optimize the resulting function.
        m_functionOptPasses->run(*fcn);


        //LOGDEBUG() << "AFTER" <<  std::endl;
        //LOGDEBUG() << *fcn << std::flush;
    }else {
        m_functionPasses->run(*ret->getFunction());
    }

    return ret;
}

}
