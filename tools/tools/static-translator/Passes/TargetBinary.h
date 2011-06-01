#ifndef REVGEN_TARGET_BINARY_H

#define REVGEN_TARGET_BINARY_H

#include <llvm/Pass.h>

#include "lib/X86Translator/Translator.h"
#include "lib/BinaryReaders/Binary.h"

namespace s2etools {
class TargetBinary : public llvm::ImmutablePass {
private:
    Binary *m_binary;
    Translator *m_translator;
public:
    static char ID;

    TargetBinary() : ImmutablePass(&ID) {
        assert( false && "Bad TargetBinary ctor used.  "
                        "Tool did not specify a TargetBinary to use?");
    }

    TargetBinary(Binary *binary, Translator *translator) : ImmutablePass(&ID){
        m_binary = binary;
        m_translator = translator;
    }

    Binary *getBinary() const {
        return m_binary;
    }

    Translator *getTranslator() const {
        return m_translator;
    }

};
}

#endif
