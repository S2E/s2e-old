#ifndef S2ETOOLS_INLINEASM_EXTR_H_

#define S2ETOOLS_INLINEASM_EXTR_H_

#include <llvm/Module.h>
#include <lib/Utils/Log.h>

#include <string>

namespace s2etools {

class InlineAssemblyExtractor {
private:
    static LogKey TAG;
    std::string m_bitcodeFile;

    llvm::Module *m_module;

    bool loadModule();

public:

    InlineAssemblyExtractor(const std::string &bitcodeFile);
    bool process();


};

}

#endif
