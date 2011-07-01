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

    AddressSet m_entryPoints;

    //Statistics
    uint64_t m_startTime;
    uint64_t m_endTime;

    bool m_extractAddresses;

    void extractAddresses(llvm::Function *llvmInstruction, bool isIndirectJump);
    bool checkString(uint64_t address, std::string &res, bool isUnicode);


    void loadLibraries();

    const AddressSet& getEntryPoints() const {
        return m_entryPoints;
    }
public:
    StaticTranslatorTool(
            const std::string &inputFile,
            const std::string &bfdFormat,
            const std::string &bitCodeLibrary,
            uint64_t entryPoint,
            bool ignoreDefaultEntrypoint);

    ~StaticTranslatorTool();
    void addEntryPoint(uint64_t ep) {
        m_entryPoints.insert(ep);
    }

    void setExtractAddresses(bool v) {
        m_extractAddresses = v;
    }

    bool translateAllInstructions();
    void computePredecessors();
    void computeFunctionEntryPoints(AddressSet &ret);
    void computeFunctionInstructions(uint64_t entryPoint, AddressSet &instructions);
    void reconstructFunctions(const AddressSet &entryPoints);
    void reconstructFunctionCalls();
    void inlineInstructions();
    void outputBitcodeFile();

    void dumpStats();

    BFDInterface *getBfd() const {
        return m_bfd;
    }

    X86Translator *getTranslator() const{
        return m_translator;
    }

};

} //translator
} //s2etools
#endif
