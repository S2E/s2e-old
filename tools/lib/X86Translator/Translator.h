/**
 * Caveats:
 *   - QEMU's translator uses global variables. It is not thread safe
 *     and only one instance can be used at a time.
 *
 *   - It is not possible to compile multiple translators in the same library.
 *     This is mainly due to function name conflicts.
 *
 * Long-term goals:
 *   - Can use multiple translators at the same time
 *   - Allow multi-threaded translation
 */

#ifndef STATIC_X86TRANSLATOR_H

#define STATIC_X86TRANSLATOR_H

#include <lib/BinaryReaders/Binary.h>
#include <llvm/System/Path.h>
#include <llvm/Function.h>


namespace s2etools
{

class InvalidAddressException {
    uintptr_t m_address;
    unsigned m_size;

public:
    InvalidAddressException(uintptr_t address, unsigned size) {
        m_address = address;
        m_size = size;
    }

    uintptr_t getAddress() const {
        return m_address;
    }
};

class TranslatorNotInitializedException {

};

class TranslatorException {
    std::string m_message;

public:
    TranslatorException(const std::string &msg) {
        m_message = msg;
    }
};

enum EBasicBlockType
{
    BB_DEFAULT=0,
    BB_JMP, BB_JMP_IND,
    BB_COND_JMP, BB_COND_JMP_IND,
    BB_CALL, BB_CALL_IND, BB_REP, BB_RET
};

struct LLVMBasicBlock
{
    /* Linear address of the basic block */
    uint64_t address;

    /* Size of the machine code */
    unsigned size;

    /* Raw LLVM representation of the machine code */
    llvm::Function *function;

    /* Type of the basic block determined by the translator */
    EBasicBlockType type;
};


class Translator {
private:
    Binary *m_binary;
    static bool s_translatorInited;

public:
    Translator(const llvm::sys::Path &bitcodeLibrary);

    virtual ~Translator();

    void setBinaryFile(Binary *binary);

    virtual LLVMBasicBlock translate(uint64_t address) = 0;

    static bool isInitialized() {
        return s_translatorInited;
    }
};



class X86Translator: public Translator {

public:
    X86Translator(const llvm::sys::Path &bitcodeLibrary);
    virtual ~X86Translator();

    virtual LLVMBasicBlock translate(uint64_t address) = 0;
};

}

#endif
