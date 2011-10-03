#include <sstream>

#include "TypeConsumer.h"
#include "lib/Utils/Utils.h"
#include <clang/AST/Type.h>
#include <clang/Basic/IdentifierTable.h>

#include <iostream>
using namespace clang;

namespace s2etools {

TypeConsumer::TypeConsumer(std::ostream &os):m_os(os)
{

}

void TypeConsumer::GetAllTypes(Types &types)
{
    unsigned i=0;
    foreach(fit, m_functionDecls.begin(), m_functionDecls.end()) {
        FunctionDecl *fd = *fit;
        foreach (pit, fd->param_begin(), fd->param_end()) {
            ParmVarDecl *param = *pit;
            clang::Type *ty = param->getType().getTypePtr();
            Types::iterator it = types.find(ty);
            if (it == types.end()) {
                //std::cerr << "i=" << i << " - ";
                //PrintType(ty, std::cerr);
                types[ty] = i;
                ++i;
            }
        }
    }
}

void TypeConsumer::DeclareType(clang::Type *type, const std::string &name)
{
    std::string declName = name;
    type->getAsStringInternal(declName, PrintingPolicy(LangOptions()));
    m_os << declName << ";" << std::endl;
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

void TypeConsumer::InvokeFunction(clang::FunctionDecl *fcn, Types &types)
{

    //We don't generate calls to noreturn functions because the code generator
    //will ignore all the stuff that follows after.    
    if (fcn->hasAttr<NoReturnAttr>()) {
    //if (fcn->getType().getNoReturnAttr()) {
        m_os << "//NORETURN: ";
    }



    m_os << fcn->getNameAsString() << "(";
    unsigned paramCount = fcn->param_size();
    unsigned currentParam = 0;

    foreach (pit, fcn->param_begin(), fcn->param_end()) {
        ParmVarDecl *param = *pit;
        QualType type = param->getType();

        Types::iterator tit = types.find(type.getTypePtr());
        assert(tit != types.end() && "Some types were not extracted.");

        unsigned varIndex = (*tit).second;
        m_os << "a" << varIndex;

        if (currentParam < paramCount - 1) {
            m_os << ", ";
        }

        ++currentParam;
    }
    m_os << ");" << std::endl;
}

void TypeConsumer::GenerateFunction()
{
    m_os << "void __invoke_all_funcs() {" << std::endl;

    Types types;
    GetAllTypes(types);

    foreach(it, types.begin(), types.end()) {
        clang::Type *ty = (*it).first;
        unsigned index = (*it).second;
        std::stringstream name;
        name << "a" << index;
        DeclareType(ty, name.str());
    }

    foreach(it, m_functionDecls.begin(), m_functionDecls.end()) {
        FunctionDecl *fcn = *it;
        InvokeFunction(fcn, types);
    }

    m_os << "}" << std::endl;
}

}
