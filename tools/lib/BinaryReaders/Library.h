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

#ifndef S2ETOOLS_LIBRARY_H

#define S2ETOOLS_LIBRARY_H

#include "ExecutableFile.h"

#include "lib/ExecutionTracer/ModuleParser.h"
#include "llvm/Support/Path.h"

#include <string>
#include <set>
#include <inttypes.h>

namespace s2etools
{

class Library
{
public:
    typedef std::map<std::string, s2etools::ExecutableFile*> ModuleNameToExec;
    typedef std::vector<std::string> PathList;
    typedef std::set<std::string> StringSet;

    Library();
    virtual ~Library();

    bool addLibrary(const std::string &libName);
    bool addLibraryAbs(const std::string &libName);

    ExecutableFile *get(const std::string &name);

    void addPath(const std::string &s);
    void setPaths(const PathList &s);

    bool print(
            const std::string &modName, uint64_t loadBase, uint64_t imageBase,
            uint64_t pc, std::string &out, bool file, bool line, bool func);

    bool print(const ModuleInstance *ni, uint64_t pc, std::string &out, bool file, bool line, bool func);
    bool getInfo(const ModuleInstance *ni, uint64_t pc, std::string &file, uint64_t &line, std::string &func);

    bool findLibrary(const std::string &libName, std::string &abspath);
    bool findSuffixedModule(const std::string &moduleName, const std::string &suffix, llvm::sys::Path &path);
    bool findBasicBlockList(const std::string &moduleName, llvm::sys::Path &path);
    bool findDisassemblyListing(const std::string &moduleName, llvm::sys::Path &path);



    static uint64_t translatePid(uint64_t pid, uint64_t pc);
private:
    PathList m_libpath;
    //std::string m_libpath;
    ModuleNameToExec m_libraries;
    StringSet m_badLibraries;

};

}

#endif
