#include "Library.h"

#include <sstream>

namespace s2etools {

BFDLibrary::BFDLibrary()
{

}

BFDLibrary::~BFDLibrary()
{
    ModuleNameToBfd::iterator it;
    for(it = m_libraries.begin(); it != m_libraries.end(); ++it) {
        delete (*it).second;
    }
}

//Add a library using a relative path
bool BFDLibrary::addLibrary(const std::string &libName)
{
    std::string s = m_libpath + "/";
    s+=libName;
    return addLibraryAbs(s);
}

//Add a library using an absolute path
bool BFDLibrary::addLibraryAbs(const std::string &libName)
{
    if (m_libraries.find(libName) != m_libraries.end()) {
        return true;
    }

    std::string ProgFile = libName;

    s2etools::BFDInterface *bfd = new s2etools::BFDInterface(ProgFile);
    bfd->initialize();
    if (!bfd->inited()) {
        delete bfd;
        return false;
    }

    m_libraries[libName] = bfd;
    return true;
}

//Get a library using a name
BFDInterface *BFDLibrary::get(const std::string &name)
{
    std::string s = m_libpath + "/";
    s+=name;

    if (!addLibrary(name)) {
        return NULL;
    }

    ModuleNameToBfd::const_iterator it = m_libraries.find(s);
    if (it == m_libraries.end()) {

        return NULL;
    }

    return (*it).second;
}

//Helper function to quickly print debug info
bool BFDLibrary::print(const ModuleInstance *mi, uint64_t pc, std::string &out, bool file, bool line, bool func)
{
    if (!mi || !mi->Mod) {
        return false;
    }

    BFDInterface *bfd = get(mi->Mod->getModuleName());
    if (!bfd) {
        return false;
    }

    uint64_t reladdr = pc - mi->LoadBase + mi->Mod->getImageBase();
    std::string source, function;
    uint64_t ln;
    if (!bfd->getInfo(reladdr, source, ln, function)) {
        return false;
    }

    std::stringstream ss;

    if (file) {
        ss << source << " ";
    }

    if (line) {
        ss << "[" << ln << "] ";
    }

    if (func) {
        ss << function;
    }

    out = ss.str();

    return true;
}

}
