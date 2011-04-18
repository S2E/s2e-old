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

#ifndef _WINDOWS_SPY_H_

#define _WINDOWS_SPY_H_

#include <s2e/Interceptor/DataStructureSpy.h>


namespace s2e {
namespace plugins {

class WindowsMonitor;

class WindowsSpy: public s2e::IDataStructureSpy
{
protected:

  static bool AddProcess(const SProcessDescriptor &ProcessOrig,
                            Processes &P);

  static void ClearProcesses(Processes &P);

  WindowsMonitor *m_OS;
public:

  WindowsSpy(WindowsMonitor *OS);
  virtual ~WindowsSpy();

  typedef enum _EWindowsVersion {
    UNKNOWN, SP1, SP2, SP3
  }EWindowsVersion;

  virtual bool ScanProcesses(Processes &P);
  
  virtual bool FindProcess(const std::string &Name,
                             const IDataStructureSpy::Processes &P,
                             SProcessDescriptor &Result);
  virtual bool FindProcess(uint64_t cr3, 
    const IDataStructureSpy::Processes &P,
    SProcessDescriptor &Result);
  
  virtual bool GetCurrentThreadStack(void *State,
    uint64_t *StackTop, uint64_t *StackBottom);

  bool ReadUnicodeString(std::string &Result,uint64_t Offset);
  static EWindowsVersion GetVersion();
};

}
}

#endif

