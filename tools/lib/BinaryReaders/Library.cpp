#include "Library.h"

#include <sstream>
#include <fstream>
#include <iostream>

namespace s2etools {

Library::Library()
{

}

Library::~Library()
{
    ModuleNameToExec::iterator it;
    for(it = m_libraries.begin(); it != m_libraries.end(); ++it) {
        delete (*it).second;
    }
}

//Add a set of library paths, separated by a colon.
void Library::setPath(const std::string &s)
{
    std::string::size_type cur=0, prev=0;

    do {
        cur = s.find(':', prev);
        m_libpath.push_back(s.substr(prev, cur));
        std::cout << "Adding libpath " << s.substr(prev, cur) << std::endl;
        prev = cur+1;
    }while(cur != std::string::npos);
}

//Cycles through the list of paths and attempts to find the specified library
bool Library::findLibrary(const std::string &libName, std::string &abspath)
{
    PathList::const_iterator it;

    for (it = m_libpath.begin(); it != m_libpath.end(); ++it) {
        std::string s = *it + "/";
        s+=libName;
        std::ifstream ifs(s.c_str());
        if (ifs.is_open()) {
            abspath = s;
            return true;
        }
    }
    return false;
}

//Add a library using a relative path
bool Library::addLibrary(const std::string &libName)
{
    std::string s;

    if (!findLibrary(libName, s)) {
        return false;
    }
    return addLibraryAbs(s);
}

//Add a library using an absolute path
bool Library::addLibraryAbs(const std::string &libName)
{
    if (m_libraries.find(libName) != m_libraries.end()) {
        return true;
    }

    std::string ProgFile = libName;

    s2etools::ExecutableFile *exec = s2etools::ExecutableFile::create(ProgFile);
    if (!exec) {
        return false;
    }

    m_libraries[libName] = exec;
    return true;
}

//Get a library using a name
ExecutableFile *Library::get(const std::string &name)
{
    std::string s;
    if (!findLibrary(name, s)) {
        return NULL;
    }

    if (!addLibraryAbs(s)) {
        return NULL;
    }

    ModuleNameToExec::const_iterator it = m_libraries.find(s);
    if (it == m_libraries.end()) {

        return NULL;
    }

    return (*it).second;
}

bool Library::getInfo(const ModuleInstance *mi, uint64_t pc, std::string &file, uint64_t &line, std::string &func)
{
    assert(mi);
    ExecutableFile *exec = get(mi->Name);
    if (!exec) {
        return false;
    }

    uint64_t reladdr = pc - mi->LoadBase + mi->ImageBase;
    if (!exec->getInfo(reladdr, file, line, func)) {
        return false;
    }

    return true;
}

//Helper function to quickly print debug info
bool Library::print(
        const std::string &modName, uint64_t loadBase, uint64_t imageBase,
        uint64_t pc, std::string &out, bool file, bool line, bool func)
{

    ExecutableFile *exec = get(modName);
    if (!exec) {
        return false;
    }

    uint64_t reladdr = pc - loadBase + imageBase;
    std::string source, function;
    uint64_t ln;
    if (!exec->getInfo(reladdr, source, ln, function)) {
        return false;
    }

    std::stringstream ss;

    if (file) {
        ss << source;
    }

    if (line) {
        ss << ":" << ln;
    }

    if (func) {
        ss << " - " << function;
    }

    out = ss.str();

    return true;
}

bool Library::print(const ModuleInstance *mi, uint64_t pc, std::string &out, bool file, bool line, bool func)
{
    if (!mi) {
        return false;
    }

    return print(mi->Name,
                 mi->LoadBase, mi->ImageBase,
                 pc, out, file, line, func);
}

}
