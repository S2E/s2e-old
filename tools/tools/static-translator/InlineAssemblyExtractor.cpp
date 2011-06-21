#include <llvm/Bitcode/ReaderWriter.h>
#include <llvm/Pass.h>
#include <llvm/PassManager.h>
#include <llvm/Analysis/Verifier.h>
#include <llvm/System/Program.h>
#include <llvm/Linker.h>
#include <llvm/Transforms/IPO.h>

#include <fstream>
#include <iomanip>

#include <lib/BinaryReaders/BFDInterface.h>
#include <lib/X86Translator/Translator.h>
#include <lib/X86Translator/TbPreprocessor.h>
#include "InlineAssemblyExtractor.h"
#include "Passes/AsmDeinliner.h"
#include "Passes/AsmNativeAdapter.h"
#include "Passes/MarkerRemover.h"
#include "StaticTranslator.h"


using namespace llvm;

namespace s2etools {
namespace translator {

LogKey InlineAssemblyExtractor::TAG = LogKey("InlineAssemblyExtractor");

InlineAssemblyExtractor::InlineAssemblyExtractor(const std::string &bitcodeFile,
                                                 const std::string &outputBitcodeFile,
                                                 const std::string &bitcodeLibrary):m_context(getGlobalContext()) {
   m_bitcodeFile = bitcodeFile;
   m_outputBitcodeFile = outputBitcodeFile;
   m_bitcodeLibrary = bitcodeLibrary;

   m_module = NULL;
   m_buffer = NULL;
}

InlineAssemblyExtractor::~InlineAssemblyExtractor() {
    if (m_module) {
        delete m_module;
    }

    if (m_buffer) {
        delete m_buffer;
    }
}

bool InlineAssemblyExtractor::loadModule()
{
    LOGINFO("Loading module " << m_bitcodeFile << std::endl);

    std::string error;
    m_buffer = MemoryBuffer::getFile(m_bitcodeFile.c_str(), &error);

    if (!m_buffer) {
        LOGERROR(error);
        return false;
    }

    m_module = ParseBitcodeFile(m_buffer, m_context, &error);
    if (!m_module) {
        LOGERROR(error);
        return false;
    }

    return true;
}

void InlineAssemblyExtractor::prepareModule()
{
    PassManager pm;
    pm.add(new AsmDeinliner());
    pm.add(createVerifierPass());
    pm.run(*m_module);
}


bool InlineAssemblyExtractor::output(llvm::Module *module, const llvm::sys::Path &path)
{
    LOGDEBUG("Writing module to " << path.toString() << std::endl);

    std::ios::openmode io_mode = std::ios::out | std::ios::trunc | std::ios::binary;

    std::ofstream outputFile(path.toString().c_str(), io_mode);
    if (!outputFile.is_open()) {
        LOGERROR("Could not open output file " << path.toString() << std::endl);
        return false;
    }

    WriteBitcodeToFile(module, outputFile);

    outputFile.close();
    return true;
}

const char **InlineAssemblyExtractor::createArguments(const std::vector<std::string> &args) const
{
    char **ret = NULL;
    ret = new char*[args.size()+1];

    for (unsigned i=0; i<args.size(); ++i) {
        ret[i] = new char[args[i].size()+1];
        strcpy(ret[i], args[i].c_str());
    }
    ret[args.size()] = NULL;

    return (const char**)(ret);
}

void InlineAssemblyExtractor::destroyArguments(const char ** args)
{
    for (unsigned i=0; args[i]; ++i) {
        delete [] args[i];
    }
    delete [] args;
}

bool InlineAssemblyExtractor::createAssemblyFile(llvm::sys::Path &outAssemblyFile, const llvm::sys::Path &inBitcodeFile)
{
    //Create a temporary assembly file
    outAssemblyFile.set("tmp");
    if (outAssemblyFile.createTemporaryFileOnDisk()) {
        LOGERROR("Could not creat assembly file" << std::endl);
        return false;
    }
    outAssemblyFile.appendSuffix("s");
    outAssemblyFile.eraseFromDisk();

    //Assemble the module

    //Invoke the code generator to output the assembly file
    llvm::sys::Path llc = llvm::sys::Program::FindProgramByName("llc");
    if (!llc.isValid()) {
        LOGERROR("Could not find llc" << std::endl);
        return false;
    }

    std::vector<std::string> argsVec;
    argsVec.push_back("llc");
    argsVec.push_back("-f");
    argsVec.push_back("-o=" + outAssemblyFile.toString());
    argsVec.push_back(inBitcodeFile.toString());

    const char **llcArgs = createArguments(argsVec);
    llvm::sys::Program assemblyGenerator;
    assemblyGenerator.ExecuteAndWait(llc, llcArgs);
    destroyArguments(llcArgs);

    if (!outAssemblyFile.exists()) {
        LOGERROR("Could not create " << outAssemblyFile.toString() << std::endl);
        return false;
    }

    return true;
}

bool InlineAssemblyExtractor::createObjectFile(llvm::sys::Path &outObjectFile, const llvm::sys::Path &inAssemblyFile)
{
    //Create a temporary object file
    outObjectFile.set("tmp");
    if (outObjectFile.createTemporaryFileOnDisk()) {
        LOGERROR("Could not creat object file" << std::endl);
        return false;
    }
    outObjectFile.eraseFromDisk();
    outObjectFile.appendSuffix("o");


    //Transform the assembly file into an object file
    std::vector<std::string> argsVec;
    argsVec.push_back("gcc");
    argsVec.push_back("-m32");
    argsVec.push_back("-c");
    argsVec.push_back("-o");
    argsVec.push_back(outObjectFile.toString());
    argsVec.push_back(inAssemblyFile.toString());

    llvm::sys::Path gcc = llvm::sys::Program::FindProgramByName("gcc");
    if (!gcc.isValid()) {
        LOGERROR("Could not find gcc" << std::endl);
        return false;
    }

    const char **gccArgs = createArguments(argsVec);

    llvm::sys::Program gccCompiler;
    gccCompiler.ExecuteAndWait(gcc, gccArgs);
    destroyArguments(gccArgs);

    if (!outObjectFile.exists()) {
        LOGERROR("Could not create " << outObjectFile.toString() << std::endl);
        return false;
    }

    return true;
}


bool InlineAssemblyExtractor::process()
{
    if (!loadModule()) {
        return false;
    }

    //Isolate all inline assembly instructions in separate functions
    prepareModule();

    LOGDEBUG(*m_module << std::endl);

    //Write the translate module to disk
    llvm::sys::Path preparedBitCodeFile("tmp");
    if (preparedBitCodeFile.createTemporaryFileOnDisk()) {
        LOGERROR("Could not create temporary file" << std::endl);
    }

    if (!output(m_module, preparedBitCodeFile)) {
        return false;
    }


    //Create assembly file
    llvm::sys::Path assemblyFile;
    if (!createAssemblyFile(assemblyFile, preparedBitCodeFile)) {
        preparedBitCodeFile.eraseFromDisk();
        return false;
    }

    //Create object file
    llvm::sys::Path objectFile;
    if (!createObjectFile(objectFile, assemblyFile)) {
        assemblyFile.eraseFromDisk();
        preparedBitCodeFile.eraseFromDisk();
        return false;
    }

    LOGDEBUG("Bitcode file: " << preparedBitCodeFile.toString() << std::endl);
    LOGDEBUG("Assembly file: " << assemblyFile.toString() << std::endl);
    LOGDEBUG("Object file: " << objectFile.toString() << std::endl);

    //Save the set of functions of the original bitcode file
    std::vector<GlobalValue*> originalFunctions;
    foreach(fit, m_module->begin(), m_module->end()) {
        LOGDEBUG("Keeping " << (*fit).getNameStr() << std::endl);
        originalFunctions.push_back(&*fit);
    }

    //Translate object file to bitcode
    if (!translateObjectFile(objectFile)) {
        LOGERROR("Failed translating object file" << std::endl);
        return false;
    }

    //Remove unused stuff from the final file
    LOGDEBUG("Cleaning output" << std::endl);
    PassManager Passes;
    Passes.add(new TargetData(m_module));
    //Passes.add(createGVExtractionPass(originalFunctions));
    Passes.add(new MarkerRemover());
    Passes.add(createGlobalDCEPass());             // Delete unreachable globals
    Passes.add(createDeadTypeEliminationPass());   // Remove dead types...
    Passes.add(createStripDeadPrototypesPass());   // Remove dead func decls
    Passes.run(*m_module);

    //Write the output
    llvm::sys::Path outputBitcodeFile(m_outputBitcodeFile);
    output(m_module, outputBitcodeFile);

    //Delete intermediate files
#if 1
    assemblyFile.eraseFromDisk();
    preparedBitCodeFile.eraseFromDisk();
    objectFile.eraseFromDisk();
#endif

    return true;
}

bool InlineAssemblyExtractor::translateObjectFile(llvm::sys::Path &inObjectFile)
{
    StaticTranslatorTool translator(inObjectFile.toString(), "", m_bitcodeLibrary, 0, true);

    DenseSet<uint64_t> entryPointsInBitcode;

    //Maps the name of the deinlined function to its
    //address in the binary object file
    Functions deinlinedFunctions;

    //Fetch all the deinlined function names from the module
    foreach(fit, m_module->begin(), m_module->end()) {
        const Function &F = *fit;
        if (F.getName().startswith("asmdein_")) {
            deinlinedFunctions[F.getNameStr()] = 0;
        }
    }

    //Fetch all the entry points that correspond to deinlined functions
    BFDInterface *bfd = translator.getBfd();

    asymbol **syms = bfd->getSymbols();
    unsigned symCount = bfd->getSymbolCount();
    unsigned foundFunctions = 0;

    for (unsigned i = 0; i<symCount; ++i) {
        //Strip the leading underscore if necessary
        std::string name;
        if (syms[i]->name[0] == '_') {
            name = &syms[i]->name[1];
        }else {
            name = syms[i]->name;
        }

        Functions::iterator it = deinlinedFunctions.find(name);
        if (it == deinlinedFunctions.end()) {
            //This is not the symbol for a deinlined function
            continue;
        }

        assert((*it).second == 0 && "Multiply defined symbols???");

        (*it).second = syms[i]->value;
        ++foundFunctions;

        LOGDEBUG("sym: " << syms[i]->name << " flags:" << std::hex << syms[i]->flags <<
                 " addr:" << syms[i]->value << std::endl);

    }

    if (foundFunctions != deinlinedFunctions.size()) {
        LOGERROR("Some functions could not be found in the object file" << std::endl);
        return false;
    }

    //Insert all the functions in the translator
    foreach(it, deinlinedFunctions.begin(), deinlinedFunctions.end()) {
        translator.addEntryPoint((*it).second);
    }

    //The object file is linked to address 0.
    //This confuses the address extractor which thinks that all
    //small integers belong to some code...
    translator.setExtractAddresses(false);

    //Perform reconstruction
    if (!translator.translateAllInstructions()) {
        return false;
    }

    translator.computePredecessors();

    StaticTranslatorTool::AddressSet entryPoints;
    translator.computeFunctionEntryPoints(entryPoints);
    translator.reconstructFunctions(entryPoints);
    translator.inlineInstructions();

    llvm::sys::Path translatedBitCode = inObjectFile;
    translatedBitCode.appendSuffix("bc");

    Module *translatedModule = translator.getTranslator()->getModule();

    output(translatedModule, translatedBitCode);


    LOGDEBUG("Translated bitcode file: " << translatedBitCode.toString() << std::endl);

    LOGDEBUG("Translated data layout: " << translatedModule->getDataLayout() << std::endl);
    LOGDEBUG("Module data layout    : " << m_module->getDataLayout() << std::endl);

    LOGDEBUG("Translated target triple: " << translatedModule->getTargetTriple() << std::endl);
    LOGDEBUG("Module target triple    : " << m_module->getTargetTriple() << std::endl);

    //Make all functions in the translated modules internal
    foreach(it, translatedModule->begin(), translatedModule->end()) {
        Function &F = *it;
        if (!F.isDeclaration()) {
            F.setLinkage(Function::InternalLinkage);
        }
    }


    //Link the translated file with the original module
    if (Linker::LinkModules(m_module, translatedModule, NULL)) {
        translatedBitCode.eraseFromDisk();
        LOGERROR("Error linking in translated module" << std::endl);
        return false;
    }

#if 0
    llvm::sys::Path linkedModule(inObjectFile);
    linkedModule.appendSuffix("bc");
    output(m_module, linkedModule);

    //Read back the module
    //This is a workaround for an LLVM bug where type equality gets broken
    //after linking the two modules. i32 != i32 :-(
    m_bitcodeFile = linkedModule.toString();
    loadModule();

    linkedModule.eraseFromDisk();
 #endif

    //Map deinlined to native
    AsmNativeAdapter::FunctionMap deinlinedToNativeMap;
    foreach(it, deinlinedFunctions.begin(), deinlinedFunctions.end()) {
        Function *deinlined = m_module->getFunction((*it).first);

        std::string nativeName = TbPreprocessor::getFunctionName((*it).second);
        Function *native = m_module->getFunction(nativeName);

        if (!native || !deinlined) {
            LOGERROR("Could not find " << nativeName << " in translated module" << std::endl);
            return false;
        }
        deinlinedToNativeMap[deinlined] = native;
    }


    PassManager pm;
    pm.add(new TargetData(m_module));
    pm.add(new AsmNativeAdapter(deinlinedToNativeMap));
    pm.add(createVerifierPass());
    pm.run(*m_module);

    return true;

}

}
}
