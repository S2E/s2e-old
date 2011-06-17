#include <llvm/Bitcode/ReaderWriter.h>
#include <llvm/Pass.h>
#include <llvm/PassManager.h>
#include <llvm/Analysis/Verifier.h>
#include <llvm/System/Program.h>

#include <fstream>
#include <iomanip>

#include "InlineAssemblyExtractor.h"
#include "Passes/AsmDeinliner.h"

using namespace llvm;

namespace s2etools {

LogKey InlineAssemblyExtractor::TAG = LogKey("InlineAssemblyExtractor");

InlineAssemblyExtractor::InlineAssemblyExtractor(const std::string &bitcodeFile,
                                                 const std::string &outputBitcodeFile) {
   m_bitcodeFile = bitcodeFile;
   m_outputBitcodeFile = outputBitcodeFile;
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


bool InlineAssemblyExtractor::output(const llvm::sys::Path &path)
{
    LOGDEBUG("Writing module to " << path.toString() << std::endl);

    std::ios::openmode io_mode = std::ios::out | std::ios::trunc | std::ios::binary;

    std::ofstream outputFile(path.toString().c_str(), io_mode);
    if (!outputFile.is_open()) {
        LOGERROR("Could not open output file " << path.toString() << std::endl);
        return false;
    }

    WriteBitcodeToFile(m_module, outputFile);

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

    if (!output(preparedBitCodeFile)) {
        return false;
    }


    llvm::sys::Path assemblyFile;
    if (!createAssemblyFile(assemblyFile, preparedBitCodeFile)) {
        preparedBitCodeFile.eraseFromDisk();
        return false;
    }

    llvm::sys::Path objectFile;
    if (!createObjectFile(objectFile, assemblyFile)) {
        assemblyFile.eraseFromDisk();
        preparedBitCodeFile.eraseFromDisk();
        return false;
    }

    LOGDEBUG("Bitcode file: " << preparedBitCodeFile.toString() << std::endl);
    LOGDEBUG("Assembly file: " << assemblyFile.toString() << std::endl);
    LOGDEBUG("Object file: " << objectFile.toString() << std::endl);

#if 0
    assemblyFile.eraseFromDisk();
    preparedBitCodeFile.eraseFromDisk();
    objectFile.eraseFromDisk();
#endif

    return true;
}

}
