#include <llvm/System/Path.h>
#include <llvm/Bitcode/ReaderWriter.h>
#include <llvm/Pass.h>
#include <llvm/PassManager.h>
#include <llvm/Analysis/Verifier.h>

#include <fstream>
#include <iomanip>

#include "InlineAssemblyExtractor.h"
#include "Passes/AsmDeinliner.h"

using namespace llvm;

namespace s2etools {

LogKey InlineAssemblyExtractor::TAG = LogKey("InlineAssemblyExtractor");

InlineAssemblyExtractor::InlineAssemblyExtractor(const std::string &bitcodeFile,
                                                 const std::string &outputBitcodeFile) {
   m_bitcodeFile = bitcodeFile;
   m_outputBitcodeFile = outputBitcodeFile;
   m_module = NULL;
   m_buffer = NULL;
}

InlineAssemblyExtractor::~InlineAssemblyExtractor() {
    if (m_module) {
        delete m_module;
    }

    if (m_buffer) {
        delete m_buffer;
    }
}

bool InlineAssemblyExtractor::loadModule()
{
    LOGINFO("Loading module " << m_bitcodeFile << std::endl);

    std::string error;
    m_buffer = MemoryBuffer::getFile(m_bitcodeFile.c_str(), &error);

    if (!m_buffer) {
        LOGERROR(error);
        return false;
    }

    m_module = ParseBitcodeFile(m_buffer, m_context, &error);
    if (!m_module) {
        LOGERROR(error);
        return false;
    }

    return true;
}

void InlineAssemblyExtractor::prepareModule()
{
    PassManager pm;
    pm.add(new AsmDeinliner());
    pm.add(createVerifierPass());
    pm.run(*m_module);
}

bool InlineAssemblyExtractor::output()
{
    LOGDEBUG("Writing module to " << m_outputBitcodeFile << std::endl);

    llvm::sys::Path path(m_outputBitcodeFile);

    std::ios::openmode io_mode = std::ios::out | std::ios::trunc | std::ios::binary;

    std::ofstream outputFile(path.toString().c_str(), io_mode);
    if (!outputFile.is_open()) {
        LOGERROR("Could not open output file " << path.toString() << std::endl);
        return false;
    }

    WriteBitcodeToFile(m_module, outputFile);

    outputFile.close();
    return true;
}

bool InlineAssemblyExtractor::process()
{
    if (!loadModule()) {
        return false;
    }

    prepareModule();

    LOGDEBUG(*m_module << std::endl);

    if (!output()) {
        return false;
    }

    return true;
}

}
