#ifndef S2ETOOLS_STATIC_TRANSLATOR_H_

#define S2ETOOLS_STATIC_TRANSLATOR_H_

#include <ostream>
#include <fstream>
#include <set>

#include <lib/BinaryReaders/Library.h>
#include <llvm/System/TimeValue.h>

#include "CFG/CBasicBlock.h"
#include "CFG/CFunction.h"

namespace s2etools {
namespace translator {



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


class StaticTranslatorTool {
public:

private:
    static bool s_translatorInited;
    BFDInterface *m_binary;
    BasicBlocks m_exploredBlocks;
    std::set<uint64_t> m_addressesToExplore;


    BasicBlocks m_functionHeaders;
    CFunctions m_functions;


    //Outputs raw x86 translated code here
    std::ofstream *m_translatedCode;
    std::ofstream m_messages;
    std::ofstream m_debug;

    //Statistics
    uint64_t m_startTime;
    uint64_t m_endTime;


    void translateBlockToX86_64(uint64_t address, void *buffer, int *codeSize);
    translator::CBasicBlock* translateBlockToLLVM(uint64_t address);

    void processTranslationBlock(CBasicBlock *bb);
    void splitExistingBlock(CBasicBlock *newBlock, CBasicBlock *existingBlock);
    void extractAddresses(CBasicBlock *bb);
    bool checkString(uint64_t address, std::string &res, bool isUnicode);

    uint64_t getEntryPoint();
public:
    StaticTranslatorTool();
    ~StaticTranslatorTool();
    void translateToX86_64();
    void exploreBasicBlocks();

    void extractFunctions();
    void reconstructFunctions();
    void renameEntryPoint();
    void cleanupCode();
    void outputBitcodeFile();

    const CFunctions &getFunctions() const {
        return m_functions;
    }

    void dumpStats();



};

} //translator
} //s2etools
#endif
