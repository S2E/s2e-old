#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Bitcode/ReaderWriter.h>

#include "InlineAssemblyExtractor.h"

using namespace llvm;

namespace s2etools {

LogKey InlineAssemblyExtractor::TAG = LogKey("InlineAssemblyExtractor");

InlineAssemblyExtractor::InlineAssemblyExtractor(const std::string &bitcodeFile) {
   m_bitcodeFile = bitcodeFile;
   m_module = NULL;
}

bool InlineAssemblyExtractor::loadModule()
{
    std::string error;
    MemoryBuffer *buffer = MemoryBuffer::getFile(m_bitcodeFile.c_str(), &error);

    if (!buffer) {
        LOGERROR(error);
        return false;
    }

    LLVMContext ctx;

    m_module = ParseBitcodeFile(buffer, ctx, &error);
    if (!m_module) {
        LOGERROR(error);
        return false;
    }

    return true;
}

bool InlineAssemblyExtractor::process()
{
    if (!loadModule()) {
        return false;
    }

    return true;
}

}
