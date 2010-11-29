extern "C" {
#include <qemu-common.h>
#include <cpu-all.h>
#include <exec-all.h>
#include <tcg/tcg.h>
#include <tcg/tcg-llvm.h>
}

#include "llvm/Support/CommandLine.h"
#include <stdio.h>
#include <inttypes.h>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>

#include <lib/BinaryReaders/BFDInterface.h>

#include "StaticTranslator.h"

using namespace llvm;
using namespace s2etools;
using namespace s2e::plugins;


namespace {
cl::opt<std::string>
    InputFile(cl::Positional, cl::Required, cl::desc("<input file>"));

cl::opt<std::string>
    OutputDir("outputdir", cl::desc("Store the analysis output in this directory"), cl::init("."));
}

namespace s2etools
{
static BFDInterface *s_currentBinary = NULL;


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

void StaticTranslatorTool::translate()
{

    CPUState env;
    TranslationBlock tb;

    QTAILQ_INIT(&env.breakpoints);
    QTAILQ_INIT(&env.watchpoints);

    env.eip = m_binary->getEntryPoint();
    if (!env.eip) {
        std::cerr << "Could not get entry point of " << InputFile << std::endl;
    }

    tb.pc = env.eip;
    tb.cs_base = 0;
    tb.tc_ptr = new uint8_t[1024];

    int codeSize = 0;
    cpu_gen_code(&env, &tb, &codeSize);

    m_translatedCode->write((const char*)tb.tc_ptr, codeSize);

    delete [] tb.tc_ptr;

}

}


int main(int argc, char** argv)
{
    cl::ParseCommandLineOptions(argc, (char**) argv);
    StaticTranslatorTool translator;
    translator.translate();
    return 0;
}
