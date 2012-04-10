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

#ifndef _MODULE_DESCRIPTOR_H_

#define _MODULE_DESCRIPTOR_H_

#include <inttypes.h>
#include <string>
#include <set>
#include <map>
#include <vector>
#include <ostream>
#include <iostream>

#include <cstring>
#include <s2e/Utils.h>

namespace s2e {

//Maps the name of the exported function to its actual address
typedef std::map<std::string,uint64_t> Exports;
  
//Maps the name of the function to its actual address
//XXX: Rename the type ImportedFunctions to ImportedSymbol.
typedef std::map<std::string, uint64_t> ImportedFunctions;
  
//Maps the library name to the set of functions it exports
typedef std::map<std::string, ImportedFunctions > Imports;

/**
 *  Defines some section of memory
 */
struct SectionDescriptor
{
    enum SectionType {
        READ=1, WRITE=2, READWRITE=3,
        EXECUTE=4
    };

    uint64_t loadBase;
    uint64_t size;
    SectionType type;
    std::string name;

    void setRead(bool b) {
        if (b) type = SectionType(type | READ);
        else type = SectionType(type & (-1 - READ));
    }

    void setWrite(bool b) {
        if (b) type = SectionType(type | WRITE);
        else type = SectionType(type & (-1 - WRITE));
    }

    void setExecute(bool b) {
        if (b) type = SectionType(type | EXECUTE);
        else type = SectionType(type & (-1 - EXECUTE));
    }

    bool isReadable() const { return type & READ; }
    bool isWritable() const { return type & WRITE; }
    bool isExecutable() const { return type & EXECUTE; }
};

typedef std::vector<SectionDescriptor> ModuleSections;

struct SymbolDescriptor {
    std::string name;
    unsigned size;

    bool operator()(const SymbolDescriptor &s1, const SymbolDescriptor &s2) const {
        return s1.name.compare(s2.name) < 0;
    }
};

typedef std::set<SymbolDescriptor, SymbolDescriptor> SymbolDescriptors;

/**
 *  Characterizes whatever module can be loaded in the memory.
 *  This can be a user-mode library, or a kernel-mode driver.
 */
struct ModuleDescriptor
{
  uint64_t  Pid;
  
  //The name of the module (eg. MYAPP.EXE or DRIVER.SYS)
  std::string Name;
  
  //Where the the prefered load address of the module.
  //This is defined by the linker and put into the header of the image.
  uint64_t NativeBase;
  
  //Where the image of the module was actually loaded by the OS.
  uint64_t LoadBase;
  
  //The size of the image of the module
  uint64_t Size;

  //The entry point of the module
  uint64_t EntryPoint;

  //A list of sections
  ModuleSections Sections;

  ModuleDescriptor() {
    Pid = 0;
    NativeBase = 0;
    LoadBase = 0;
    Size = 0;
    EntryPoint = 0;
  }

  bool Contains(uint64_t RunTimeAddress) const {
    uint64_t RVA = RunTimeAddress - LoadBase;
    return RVA < Size;
  }

  uint64_t ToRelative(uint64_t RunTimeAddress) const {
    uint64_t RVA = RunTimeAddress - LoadBase;
    return RVA;
  }

  uint64_t ToNativeBase(uint64_t RunTimeAddress) const {
    return RunTimeAddress - LoadBase + NativeBase;
  }

  uint64_t ToRuntime(uint64_t NativeAddress) const {
    return NativeAddress - NativeBase + LoadBase;
  }

  bool EqualInsensitive(const char *Name) const{
	return strcasecmp(this->Name.c_str(), Name) == 0;
  }

  struct ModuleByLoadBase {
    bool operator()(const struct ModuleDescriptor& s1, 
      const struct ModuleDescriptor& s2) const {
        if (s1.Pid == s2.Pid) {
            return s1.LoadBase + s1.Size <= s2.LoadBase;
        }
        return s1.Pid < s2.Pid;
    }

    bool operator()(const struct ModuleDescriptor* s1,
      const struct ModuleDescriptor* s2) const {
        if (s1->Pid == s2->Pid) {
            return s1->LoadBase + s1->Size <= s2->LoadBase;
        }
        return s1->Pid < s2->Pid;
    }
  };

  struct ModuleByName {
    bool operator()(const struct ModuleDescriptor& s1,
      const struct ModuleDescriptor& s2) const {
        return s1.Name < s2.Name;
    }

    bool operator()(const struct ModuleDescriptor* s1,
      const struct ModuleDescriptor* s2) const {
        return s1->Name < s2->Name;
    }
  };

  void Print(llvm::raw_ostream &os) const {
    os << "Name=" << Name  <<
      " NativeBase=" << hexval(NativeBase) << " LoadBase=" << hexval(LoadBase) <<
      " Size=" << hexval(Size) <<
      " EntryPoint=" << hexval(EntryPoint) << '\n';
  }


  typedef std::set<struct ModuleDescriptor, ModuleByLoadBase> MDSet;
};

}

#endif
