#ifndef TYPE_EXTRACTOR_TOOL_TC

#define TYPE_EXTRACTOR_TOOL_TC

#include <clang/AST/ASTConsumer.h>
#include <clang/AST/Decl.h>
#include <clang/AST/DeclGroup.h>
#include <clang/AST/ASTContext.h>

#include <vector>
#include <set>
#include <ostream>

namespace s2etools {

class TypeConsumer : public clang::ASTConsumer {
public:
    typedef std::vector<clang::FunctionDecl*> FunctionDecls;
    typedef std::set<clang::Type *> Types;
private:
    FunctionDecls m_functionDecls;
    std::ostream &m_os;

    void DeclareFunction(clang::FunctionDecl *VD);
    void DeclareType(clang::Type *type, const std::string &name);
    void GetAllTypes(Types &types);

public:
    TypeConsumer(std::ostream &os);

    void GenerateFunction();

    virtual void HandleTopLevelDecl(clang::DeclGroupRef D);
};

}

#endif
