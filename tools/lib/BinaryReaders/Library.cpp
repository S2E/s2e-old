/*
 * S2E Selective Symbolic Execution Framework
 *
 * Copyright (c) 2010, Dependable Systems Laboratory, EPFL
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Dependable Systems Laboratory, EPFL nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE DEPENDABLE SYSTEMS LABORATORY, EPFL BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Currently maintained by:
 *    Vitaly Chipounov <vitaly.chipounov@epfl.ch>
 *    Volodymyr Kuznetsov <vova.kuznetsov@epfl.ch>
 *
 * All contributors are listed in S2E-AUTHORS file.
 *
 */

#include "Library.h"

#include <sstream>
#include <fstream>
#include <iostream>

#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/FileSystem.h"


//XXX: Move this to a better place
namespace {
    llvm::cl::opt<uint64_t>
            KernelStart("os", llvm::cl::desc("Start address of kernel space"),llvm::cl::init(0x80000000));
}

namespace s2etools {

uint64_t Library::translatePid(uint64_t pid, uint64_t pc)
{
    if (pc >= KernelStart) {
        return 0;
    }
    return pid;
}

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

void Library::addPath(const std::string &path)
{
    m_libpath.push_back(path);
}

void Library::setPaths(const PathList &s)
{
    m_libpath.clear();
    m_libpath = s;
}

//Cycles through the list of paths and attempts to find the specified library
bool Library::findLibrary(const std::string &libName, std::string &abspath)
{
    PathList::const_iterator it;

    for (it = m_libpath.begin(); it != m_libpath.end(); ++it) {
        llvm::sys::Path lib(*it);
        lib.appendComponent(libName);

        bool exists = false;
        llvm::sys::fs::exists(lib.str(), exists);

        if (exists) {
            abspath = lib.str();
            return true;
        }
    }
    return false;
}

bool Library::findSuffixedModule(const std::string &moduleName, const std::string &suffix, llvm::sys::Path &path)
{
    PathList::const_iterator it;

    for (it = m_libpath.begin(); it != m_libpath.end(); ++it) {
        llvm::sys::Path list(*it);
        list.appendComponent(moduleName);
        list.appendSuffix(suffix);

        bool exists = false;
        llvm::sys::fs::exists(list.str(), exists);
        if (exists) {
            path = list;
            return true;
        }
    }
    return false;
}

bool Library::findBasicBlockList(const std::string &moduleName, llvm::sys::Path &path)
{
    return findSuffixedModule(moduleName, "bblist", path);
}

bool Library::findDisassemblyListing(const std::string &moduleName, llvm::sys::Path &path)
{
    return findSuffixedModule(moduleName, "lst", path);
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

    if (m_badLibraries.find(libName) != m_badLibraries.end()) {
        return false;
    }

    std::string ProgFile = libName;

    s2etools::ExecutableFile *exec = s2etools::ExecutableFile::create(ProgFile);
    if (!exec) {
        m_badLibraries.insert(ProgFile);
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
    if(!mi)
        return false;
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
