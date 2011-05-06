#ifndef S2ETOOLS_STATIC_TRANSLATOR_H_

#define S2ETOOLS_STATIC_TRANSLATOR_H_

#include <ostream>
#include <fstream>
#include <set>

#include <lib/BinaryReaders/Library.h>
#include <lib/Utils/ExperimentManager.h>
#include <llvm/System/TimeValue.h>

#include "CFG/CBasicBlock.h"
#include "CFG/CFunction.h"
#include <lib/X86Translator/Translator.h>

namespace s2etools {
namespace translator {


class StaticTranslatorTool {
public:

private:
    std::string TAG;
    static bool s_translatorInited;
    BFDInterface *m_bfd;
    Binary *m_binary;
    X86Translator *m_translator;

    ExperimentManager *m_experiment;

    BasicBlocks m_exploredBlocks;
    std::set<uint64_t> m_addressesToExplore;


    BasicBlocks m_functionHeaders;
    CFunctions m_functions;


    //Outputs raw x86 translated code here
    std::ostream *m_translatedCode;

    //Statistics
    uint64_t m_startTime;
    uint64_t m_endTime;

    translator::CBasicBlock* translateBlockToLLVM(uint64_t address);

    void processTranslationBlock(CBasicBlock *bb);
    void splitExistingBlock(CBasicBlock *newBlock, CBasicBlock *existingBlock);
    void extractAddresses(CBasicBlock *bb);
    bool checkString(uint64_t address, std::string &res, bool isUnicode);

    uint64_t getEntryPoint();
public:
    StaticTranslatorTool();
    ~StaticTranslatorTool();
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
