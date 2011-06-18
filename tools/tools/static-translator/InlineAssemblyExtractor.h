#ifndef S2ETOOLS_INLINEASM_EXTR_H_

#define S2ETOOLS_INLINEASM_EXTR_H_

#include <llvm/Module.h>
#include <llvm/LLVMContext.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/System/Path.h>
#include <lib/Utils/Log.h>

#include <string>
#include <map>

namespace s2etools {
namespace translator {

class InlineAssemblyExtractor {
private:
    typedef std::map<std::string, uint64_t> Functions;

    static LogKey TAG;
    std::string m_bitcodeFile;
    std::string m_outputBitcodeFile;
    std::string m_bitcodeLibrary;

    llvm::LLVMContext &m_context;
    llvm::MemoryBuffer *m_buffer;
    llvm::Module *m_module;

    bool loadModule();
    void prepareModule();
    bool output(llvm::Module *module, const llvm::sys::Path &path);

    const char **createArguments(const std::vector<std::string> &args) const;
    void destroyArguments(const char ** args);

    bool createAssemblyFile(llvm::sys::Path &outAssemblyFile, const llvm::sys::Path &inBitcodeFile);
    bool createObjectFile(llvm::sys::Path &outObjectFile, const llvm::sys::Path &inAssemblyFile);
    bool translateObjectFile(llvm::sys::Path &inObjectFile);

public:

    InlineAssemblyExtractor(const std::string &bitcodeFile,
                            const std::string &outputBitcodeFile,
                            const std::string &bitcodeLibrary);
    ~InlineAssemblyExtractor();
    bool process();


};

}
}

#endif
