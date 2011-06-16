#ifndef S2ETOOLS_INLINEASM_EXTR_H_

#define S2ETOOLS_INLINEASM_EXTR_H_

#include <llvm/Module.h>
#include <llvm/LLVMContext.h>
#include <llvm/Support/MemoryBuffer.h>
#include <lib/Utils/Log.h>

#include <string>

namespace s2etools {

class InlineAssemblyExtractor {
private:
    static LogKey TAG;
    std::string m_bitcodeFile;
    std::string m_outputBitcodeFile;

    llvm::LLVMContext m_context;
    llvm::MemoryBuffer *m_buffer;
    llvm::Module *m_module;

    bool loadModule();
    void prepareModule();
    bool output();

public:

    InlineAssemblyExtractor(const std::string &bitcodeFile,
                            const std::string &outputBitcodeFile);
    ~InlineAssemblyExtractor();
    bool process();


};

}

#endif
