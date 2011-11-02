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

#ifndef __WINDOWS_UM_INTERCEPTOR_H__

#define __WINDOWS_UM_INTERCEPTOR_H__

#include <s2e/Plugins/ModuleDescriptor.h>

#include <s2e/Plugin.h>
#include <s2e/Plugins/CorePlugin.h>
#include <s2e/Plugins/OSMonitor.h>


#include "WindowsMonitor.h"

namespace s2e {
namespace plugins {

class WindowsUmInterceptor
{
private:
  WindowsMonitor *m_Os;

  typedef std::set<ModuleDescriptor*> HookedLibrarySet;
  typedef std::set<std::string> StringSet;


  uint64_t m_ASBase;
  uint64_t m_ASSize;
  
  enum EUmTracingState {
    SEARCH_PROCESS,
    WAIT_PROCESS_INIT,
    WAIT_LIBRARY_LOAD,
    LOAD_DONE
  };

  enum EUmTracingState m_TracingState;

  uint64_t m_Cr3;
  uint64_t m_PrevCr3;
  uint64_t m_HookedCr3;
  uint64_t m_LdrAddr; 
  uint64_t m_ProcBase;
  
  ModuleDescriptor::MDSet m_LoadedLibraries;

  //IDataStructureSpy::Processes m_ProcList;

  std::string m_ProcessName;
  std::map<std::string, ModuleDescriptor> m_Modules;

  bool WaitForProcessInit(S2EExecutionState *state);
  bool FindModules(S2EExecutionState *State);
  bool InitImports();

  void NotifyLoadedProcesses(S2EExecutionState *state);
  void NotifyModuleLoad(S2EExecutionState *state, ModuleDescriptor &Library);

  bool CatchModuleUnloadBase(S2EExecutionState *State, uint64_t pLdrEntry);
  bool CatchModuleUnloadServer2008(S2EExecutionState *State);
  bool CatchModuleUnloadXPSP3(S2EExecutionState *State);

  bool CatchProcessTerminationXp(S2EExecutionState *State);
  bool CatchProcessTerminationServer2008(S2EExecutionState *State);

public:
  WindowsUmInterceptor(WindowsMonitor *Monitor);
  virtual ~WindowsUmInterceptor();

  bool CatchModuleLoad(S2EExecutionState *State);
  bool CatchProcessTermination(S2EExecutionState *State);
  bool CatchModuleUnload(S2EExecutionState *State);
  
  bool GetPids(S2EExecutionState *State, PidSet &out);
};


}
}

#endif
