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

#ifndef _DATA_STRUCTURE_SPY_

#define _DATA_STRUCTURE_SPY_

#include <string>
#include <set>
#include <inttypes.h>

namespace s2e
{

typedef struct _SProcessDescriptor
{
  uint64_t PageDirectory;
  std::string Name;
}SProcessDescriptor;

struct IDataStructureSpy
{
  /*********************************************/
  struct ProcessByDir {
    bool operator()(const struct _SProcessDescriptor& s1, 
      const struct _SProcessDescriptor& s2) const {
      return s1.PageDirectory < s2.PageDirectory;
    }
  };

  struct ProcessByName {
    bool operator()(const struct _SProcessDescriptor& s1, 
      const struct _SProcessDescriptor& s2) const {
      return s1.Name < s2.Name;
    }
  };

  /*********************************************/

public:
  typedef std::multiset<SProcessDescriptor, ProcessByName> ProcessesByName;
  typedef std::set<SProcessDescriptor, ProcessByDir> ProcessesByDir;

  typedef struct _Processes {
    ProcessesByName ByName;
    ProcessesByDir ByDir;
  }Processes;
public:
  
  virtual bool ScanProcesses(Processes &P)=0;
  virtual bool FindProcess(const std::string &Name,
                             const IDataStructureSpy::Processes &P,
                             SProcessDescriptor &Result) = 0;
  virtual bool FindProcess(uint64_t cr3, 
    const IDataStructureSpy::Processes &P,
    SProcessDescriptor &Result) = 0;
  
  virtual bool GetCurrentThreadStack(void *State,
    uint64_t *StackTop, uint64_t *StackBottom)=0;
};

}

#endif
