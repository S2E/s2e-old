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

#ifndef __MODULE_MONITOR_PLUGIN_H__

#define __MODULE_MONITOR_PLUGIN_H__

#include <s2e/Plugin.h>
#include <s2e/S2EExecutionState.h>
#include "ModuleDescriptor.h"
#include "ThreadDescriptor.h"

namespace s2e {
namespace plugins {

/**
 *  Base class for default OS actions.
 *  It provides an interface for loading/unloading modules and processes.
 *  If you wish to add support for a new OS, implement this interface.
 *
 *  Note: several events use ModuleDescriptor as a parameter.
 *  The passed reference is valid only during the call. Do not store pointers
 *  to such objects, but make a copy instead.
 */
class OSMonitor:public Plugin
{
public:
   sigc::signal<void,
      S2EExecutionState*,
      const ModuleDescriptor &
   >onModuleLoad;

   sigc::signal<void, S2EExecutionState*, const ModuleDescriptor &> onModuleUnload;
   sigc::signal<void, S2EExecutionState*, uint64_t> onProcessUnload;

   sigc::signal<void, S2EExecutionState*, const ThreadDescriptor&> onThreadCreate;
   sigc::signal<void, S2EExecutionState*, const ThreadDescriptor&> onThreadExit;
protected:
   OSMonitor(S2E* s2e): Plugin(s2e) {}

public:
   virtual bool getImports(S2EExecutionState *s, const ModuleDescriptor &desc, Imports &I) = 0;
   virtual bool getExports(S2EExecutionState *s, const ModuleDescriptor &desc, Exports &E) = 0;
   virtual bool isKernelAddress(uint64_t pc) const = 0;
   virtual uint64_t getPid(S2EExecutionState *s, uint64_t pc) = 0;
   virtual bool getCurrentStack(S2EExecutionState *s, uint64_t *base, uint64_t *size) = 0;

   bool isOnTheStack(S2EExecutionState *s, uint64_t address) {
       uint64_t base, size;
       if (!getCurrentStack(s, &base, &size)) {
           return false;
       }
       return address >= base && address < (base + size);
   }

};

} // namespace plugins
} // namespace s2e

#endif
