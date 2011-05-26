#ifndef S2ETOOLS_STATIC_TRANSLATOR_H_

#define S2ETOOLS_STATIC_TRANSLATOR_H_

#include <ostream>
#include <fstream>
#include <set>

#include <lib/BinaryReaders/Library.h>
#include <lib/Utils/ExperimentManager.h>
#include <llvm/System/TimeValue.h>
#include <llvm/ADT/DenseMap.h>
#include <llvm/ADT/DenseSet.h>

#include "CFG/CFunction.h"
#include <lib/X86Translator/Translator.h>
#include <lib/Utils/Utils.h>
#include <lib/Utils/Log.h>

namespace s2etools {
namespace translator {


class StaticTranslatorTool {
public:
    typedef std::set<TranslatedBlock> TranslatedInstructions;
    typedef llvm::DenseMap<uint64_t, unsigned> CounterMap;
    typedef llvm::DenseMap<uint64_t, TranslatedBlock *> TranslatedBlocksMap;
    typedef llvm::DenseSet<uint64_t> AddressSet;
private:
    static LogKey TAG;
    static bool s_translatorInited;
    BFDInterface *m_bfd;
    Binary *m_binary;
    X86Translator *m_translator;

    ExperimentManager *m_experiment;

    llvm::DenseSet<uint64_t> m_addressesToExplore;
    llvm::DenseSet<uint64_t> m_exploredAddresses;

    TranslatedBlocksMap m_translatedInstructions;

    CounterMap m_predecessors;

    BasicBlocks m_functionHeaders;
    CFunctions m_functions;


    //Outputs raw x86 translated code here
    std::ostream *m_translatedCode;

    //Statistics
    uint64_t m_startTime;
    uint64_t m_endTime;

    void extractAddresses(llvm::Function *llvmInstruction, bool isIndirectJump);
    bool checkString(uint64_t address, std::string &res, bool isUnicode);


    void loadLibraries();

    uint64_t getEntryPoint();
public:
    StaticTranslatorTool();
    ~StaticTranslatorTool();
    void translateAllInstructions();
    void computePredecessors();
    void computeFunctionEntryPoints(AddressSet &ret);
    void computeFunctionInstructions(uint64_t entryPoint, AddressSet &instructions);
    void reconstructFunctions(const AddressSet &entryPoints);
    void reconstructFunctionCalls();
    void outputBitcodeFile();

    void dumpStats();



};

} //translator
} //s2etools
#endif
