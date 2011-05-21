#ifndef TYPE_EXTRACTOR_TOOL

#define TYPE_EXTRACTOR_TOOL

#include <clang/Basic/TargetInfo.h>
#include <clang/Basic/FileManager.h>
#include <clang/Basic/LangOptions.h>
#include <clang/Basic/SourceManager.h>

#include <llvm/System/Path.h>

#include  <string>

namespace s2etools {

class TypeConsumer;

class TypeExtractor {
private:
    std::string m_inputFile;
    std::string m_errorFile;

    llvm::sys::Path m_tempFile;
    std::ostream *m_os;


    clang::LangOptions m_langOptions;
    clang::SourceManager m_sourceManager;
    clang::FileManager m_fileManager;

    clang::TargetInfo *m_targetInfo;

    TypeConsumer *m_typeConsumer;
public:
    TypeExtractor(const std::string &inputFile,
                  const std::string &errorFile);
    ~TypeExtractor();

    bool ExtractTypes();

};
}

#endif
