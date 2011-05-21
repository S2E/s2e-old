#include <sstream>

#include "TypeConsumer.h"
#include "lib/Utils/Utils.h"

using namespace clang;

namespace s2etools {

TypeConsumer::TypeConsumer(std::ostream &os):m_os(os)
{

}

void TypeConsumer::DeclareFunction(FunctionDecl *VD)
{
    m_os << VD->getNameAsString() << "(";

    foreach (pit, VD->param_begin(), VD->param_end()) {
        ParmVarDecl *param = *pit;
        QualType type = param->getType();
        if (const TypedefType *td = type->getAsTypedefType()) {
            m_os << td->getDecl()->getNameAsCString() << ", ";
        }else if (const BuiltinType *td = type->getAsBuiltinType()) {
            m_os << td->getName(LangOptions()) << ", ";
        }
    }
    m_os << ")" << std::endl;
}

void TypeConsumer::GetAllTypes(Types &types)
{
    foreach(fit, m_functionDecls.begin(), m_functionDecls.end()) {
        FunctionDecl *fd = *fit;
        foreach (pit, fd->param_begin(), fd->param_end()) {
            ParmVarDecl *param = *pit;
            types.insert(param->getType().getTypePtr());
        }
    }
}

void TypeConsumer::DeclareType(clang::Type *type, const std::string &name)
{
    if (const TypedefType *td = type->getAsTypedefType()) {
        m_os << td->getDecl()->getNameAsCString();
    }else if (const BuiltinType *td = type->getAsBuiltinType()) {
        m_os << td->getName(LangOptions());
    }else {
        return;
    }
    m_os << " " << name << ";" << std::endl;
}

void TypeConsumer::HandleTopLevelDecl(DeclGroupRef D)
{
    static int count = 0;
    DeclGroupRef::iterator it;
    for(it = D.begin(); it != D.end(); it++) {
        count++;
        //std::cout << "count: " << count << std::endl;
        FunctionDecl *VD = dyn_cast<FunctionDecl>(*it);
        if(!VD)
            continue;

        m_functionDecls.push_back(VD);
    }
}

void TypeConsumer::GenerateFunction()
{
    m_os << "void __invoke_all_funcs() {" << std::endl;

    Types types;
    GetAllTypes(types);
    unsigned i=0;
    foreach(it, types.begin(), types.end()) {
        clang::Type *ty = *it;
        std::stringstream name;
        name << "a" << i;
        DeclareType(ty, name.str());
        ++i;
    }

    m_os << "}" << std::endl;
}

}
