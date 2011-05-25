#include "TypeExtractor.h"

#include <llvm/Support/CommandLine.h>
#include <llvm/Module.h>

#include "clang/Frontend/ASTConsumers.h"
#include "clang/Frontend/InitHeaderSearch.h"
#include "clang/Basic/FileManager.h"
#include "clang/Basic/Builtins.h"
#include "clang/Basic/IdentifierTable.h"
#include "clang/Frontend/TextDiagnosticBuffer.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"
#include "clang/Frontend/PCHReader.h"

#include "clang/Frontend/InitPreprocessor.h"
#include "clang/Frontend/CompileOptions.h"
#include "clang/CodeGen/ModuleBuilder.h"
#include "clang/Basic/TargetInfo.h"
#include "clang/Lex/HeaderSearch.h"
#include "clang/Lex/Preprocessor.h"
#include "llvm/System/Host.h"
#include "llvm/Support/Streams.h"

#include <clang/Parse/Action.h>
#include <clang/Parse/Parser.h>

#include <clang/Sema/ParseAST.h>


#include "lib/Utils/Utils.h"

#include "TypeConsumer.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>

using namespace llvm;
using namespace clang;
using namespace s2etools;

namespace {
cl::opt<std::string>
    InputFile(cl::Positional, cl::Required, cl::desc("<input file>"));

cl::opt<std::string>
    OuputSourceFile("outsrc", cl::desc("Where to write generated source file"), cl::init("output.c"));

cl::opt<std::string>
    ErrorFile("errlog", cl::desc("Where to write compiler errors"), cl::init("errors.txt"));

cl::list<std::string>
    IncludeDirs("I", llvm::cl::value_desc("include-dir"), llvm::cl::Prefix, llvm::cl::desc("Include directory"));

cl::list<std::string>
    Defines("D", llvm::cl::value_desc("defines"), llvm::cl::Prefix, llvm::cl::desc("Preprocessor macros"));


}


namespace s2etools {

TypeExtractor::TypeExtractor(const std::string &inputFile,
                             const std::string &errorFile,
                             const std::string &outputFile):
                                m_inputFile(inputFile),
                                m_errorFile(errorFile),
                                m_outputFile(outputFile)
{
    m_langOptions.GNUMode = 1;
    m_langOptions.C99 = 1;
    m_langOptions.NeXTRuntime = 1;
    m_langOptions.Blocks = 1;
    m_langOptions.MathErrno = 0;
    m_langOptions.PICLevel = 1;
    m_langOptions.NoInline = 1;
    m_langOptions.EmitAllDecls = 1;

    m_targetInfo = TargetInfo::CreateTargetInfo("i386-apple-darwin10.0");

    std::string ErrMsg;
    m_tempFile.set(outputFile);
#if 0

    if (m_tempFile.createTemporaryFileOnDisk(false, &ErrMsg)) {
        std::cerr << "Could not create temporary file " << m_tempFile.c_str() << std::endl;
        std::cerr << ErrMsg << std::endl;
        exit(-1);
    }
#endif

    std::ios::openmode io_mode = std::ios::out | std::ios::trunc | std::ios::binary;
    m_os = new std::ofstream(m_tempFile.c_str(), io_mode);

    m_typeConsumer = new TypeConsumer(*m_os);
}

TypeExtractor::~TypeExtractor() {
    //m_tempFile.eraseFromDisk(true, NULL);
    delete m_os;
}

void TypeExtractor::GenerateSourceHeader()
{
    foreach (mit, Defines.begin(), Defines.end()) {
        *m_os << "#define " << *mit << std::endl;
    }

    *m_os << "#include \"" << InputFile << "\"" << std::endl << std::endl;
}

bool TypeExtractor::ExtractTypes()
{
    std::string errorInfo = "";
    llvm::raw_fd_ostream errorStream(m_errorFile.c_str(), true, true, errorInfo);

    TextDiagnosticPrinter diagPrinter(errorStream);
    Diagnostic diag(&diagPrinter);

    HeaderSearch headers(m_fileManager);
    InitHeaderSearch init(headers);

    foreach(iit, IncludeDirs.begin(), IncludeDirs.end()) {
        init.AddPath(*iit, InitHeaderSearch::Angled, false, true, false);
    }

#if 0
    init.AddPath("/Users/vitaly/softs/mingw/include", InitHeaderSearch::Angled, false, true, false);
    init.AddPath("/Users/vitaly/softs/mingw//lib/gcc/i386-mingw32/4.2.1-sjlj/include", InitHeaderSearch::Angled, false, true, false);
#endif

    init.Realize();

    diagPrinter.setLangOptions(&m_langOptions);

    Preprocessor pp(diag, m_langOptions, *m_targetInfo, m_sourceManager, headers);

    PreprocessorInitOptions ppio;

    foreach (mit, Defines.begin(), Defines.end()) {
        ppio.addMacroDef(*mit);
    }

    InitializePreprocessor(pp, ppio);

    const FileEntry *file = m_fileManager.getFile(m_inputFile);
    m_sourceManager.createMainFileID(file, SourceLocation());

    IdentifierTable tab(m_langOptions);
    SelectorTable sel;
    Builtin::Context builtins(*m_targetInfo);

    ASTContext ctx(m_langOptions, m_sourceManager, *m_targetInfo, tab, sel, builtins);
    ParseAST(pp, m_typeConsumer, ctx, false, true);

    if (!diag.hasErrorOccurred()) {
        GenerateSourceHeader();
        m_typeConsumer->GenerateFunction();
    }

    return diag.hasErrorOccurred();
}

}



int main(int argc, char **argv)
{
    cl::ParseCommandLineOptions(argc, (char**) argv);

    TypeExtractor te(InputFile, ErrorFile, OuputSourceFile);
    te.ExtractTypes();

#if 0
    CompileOptions CompOpts;


    CodeGenerator *codegen = CreateLLVMCodeGen(diag, "output.bc", CompOpts,
                                               llvm::getGlobalContext());

    ASTContext ctx(lang, sm, *ti, tab, sel, builtins);
    ParseAST(pp, codegen, ctx, true, true);

    if (diag.hasErrorOccurred()) {
        std::cerr << "Errors have occured" << std::endl;
        return -1;
    }

    std::cerr << *codegen->GetModule() << std::endl;
#endif
    return 0;

}


