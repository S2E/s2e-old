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

/**
 *  This plugin provides the means of manually specifying the location
 *  of modules in memory.
 *
 *  This allows things like defining poritions of the BIOS.
 *
 *  RESERVES THE CUSTOM OPCODE 0xAA
 */

extern "C" {
#include "config.h"
#include "qemu-common.h"
}


#include <s2e/S2E.h>
#include <s2e/Utils.h>
#include <s2e/ConfigFile.h>
#include <s2e/Plugins/Opcodes.h>
#include "RawMonitor.h"

#include <sstream>

using namespace std;

using namespace s2e;
using namespace s2e::plugins;

S2E_DEFINE_PLUGIN(RawMonitor, "Plugin for monitoring raw module events", "Interceptor");

RawMonitor::~RawMonitor()
{

}

bool RawMonitor::initSection(const std::string &cfgKey, const std::string &svcId)
{
    Cfg cfg;

    bool ok;
    cfg.name = s2e()->getConfig()->getString(cfgKey + ".name", "", &ok);
    if (!ok) {
        s2e()->getWarningsStream() << "You must specify " << cfgKey <<  ".name" << std::endl;
        return false;
    }

    cfg.size = s2e()->getConfig()->getInt(cfgKey + ".size", 0, &ok);
    if (!ok) {
        s2e()->getWarningsStream() << "You must specify " << cfgKey <<  ".size" << std::endl;
        return false;
    }

    cfg.start = s2e()->getConfig()->getInt(cfgKey + ".start", 0, &ok);
    if (!ok) {
        s2e()->getWarningsStream() << "You must specify " << cfgKey <<  ".start" << std::endl;
        return false;
    }

    cfg.nativebase = s2e()->getConfig()->getInt(cfgKey + ".nativebase", 0, &ok);
    if (!ok) {
        s2e()->getWarningsStream() << "You must specify " << cfgKey <<  ".nativebase" << std::endl;
        return false;
    }

    cfg.delayLoad = s2e()->getConfig()->getBool(cfgKey + ".delay", false, &ok);
    if (!ok) {
        s2e()->getWarningsStream() << "You must specify " << cfgKey <<  ".delay" << std::endl;
        return false;
    }

    cfg.kernelMode = s2e()->getConfig()->getBool(cfgKey + ".kernelmode", false, &ok);
    if (!ok) {
        s2e()->getWarningsStream() << "You must specify " << cfgKey <<  ".kernelmode" << std::endl;
        return false;
    }

    m_cfg.push_back(cfg);
    return true;
}

void RawMonitor::initialize()
{
    std::vector<std::string> Sections;
    Sections = s2e()->getConfig()->getListKeys(getConfigKey());
    bool noErrors = true;

    bool ok = false;
    m_kernelStart = s2e()->getConfig()->getInt(getConfigKey() + ".kernelStart", 0, &ok);
    if (!ok) {
        s2e()->getWarningsStream() << "You must specify " << getConfigKey() << ".kernelStart" << std::endl;
        exit(-1);
    }

    foreach2(it, Sections.begin(), Sections.end()) {
        if (*it == "kernelStart") {
            continue;
        }

        s2e()->getMessagesStream() << "Scanning section " << getConfigKey() << "." << *it << std::endl;
        std::stringstream sk;
        sk << getConfigKey() << "." << *it;
        if (!initSection(sk.str(), *it)) {
            noErrors = false;
        }
    }

    if (!noErrors) {
        s2e()->getWarningsStream() << "Errors while scanning the RawMonitor sections"
            <<std::endl;
        exit(-1);
    }

    m_onTranslateInstruction = s2e()->getCorePlugin()->onTranslateInstructionStart.connect(
        sigc::mem_fun(*this, &RawMonitor::onTranslateInstructionStart));

    s2e()->getCorePlugin()->onCustomInstruction.connect(
            sigc::mem_fun(*this, &RawMonitor::onCustomInstruction));

}

void RawMonitor::onCustomInstruction(S2EExecutionState* state, uint64_t opcode)
{
    if (!OPCODE_CHECK(opcode, RAW_MONITOR_OPCODE)) {
        return;
    }

    opcode >>= 16;
    uint8_t op = opcode & 0xFF;
    opcode >>= 8;

    switch(op) {
    case 0:
        {
            //Module load
            //eax = pointer to module name
            //ebx = runtime load base
            //ecx = entry point
            uint32_t rtloadbase, name, size;
            bool ok = true;

#ifdef TARGET_ARM
            ok &= state->readCpuRegisterConcrete(CPU_OFFSET(regs[0]),
                                                 &name, 4);
            ok &= state->readCpuRegisterConcrete(CPU_OFFSET(regs[1]),
                                                 &rtloadbase, 4);
            ok &= state->readCpuRegisterConcrete(CPU_OFFSET(regs[2]),
                                                 &size, 4);
#elif defined(TARGET_I386)
            ok &= state->readCpuRegisterConcrete(CPU_OFFSET(regs[R_EAX]),
                                                 &name, 4);
            ok &= state->readCpuRegisterConcrete(CPU_OFFSET(regs[R_EBX]),
                                                 &rtloadbase, 4);
            ok &= state->readCpuRegisterConcrete(CPU_OFFSET(regs[R_ECX]),
                                                 &size, 4);
#else
            assert(false);
#endif
            if(!ok) {
                s2e()->getWarningsStream(state)
                    << "ERROR: symbolic argument was passed to s2e_op "
                       "rawmonitor loadmodule" << std::endl;
                break;
            }

            std::string nameStr;
            if(!state->readString(name, nameStr)) {
                s2e()->getWarningsStream(state)
                        << "Error reading module name string from the guest" << std::endl;
                return;
            }

            //Look for the module in the config section and update its load address
            CfgList::iterator it;

            for (it = m_cfg.begin(); it != m_cfg.end(); ++it) {
                Cfg &c = *it;
                if (c.name == nameStr) {
                    s2e()->getMessagesStream() << "RawMonitor: Registering " << nameStr << " "
                            " @0x" << std::hex << rtloadbase << " size=0x" << size  << std::endl;
                    c.start = rtloadbase;
                    c.size = size;
                    loadModule(state, c, false);
                }
            }
        }
        break;

    case 1:
        {
            //Specifying a new import descriptor
            uint32_t dllname, funcname, funcptr;
            bool ok = true;

#ifdef TARGET_ARM
            ok &= state->readCpuRegisterConcrete(CPU_OFFSET(regs[0]),
                                                 &dllname, 4);
            ok &= state->readCpuRegisterConcrete(CPU_OFFSET(regs[1]),
                                                 &funcname, 4);
            ok &= state->readCpuRegisterConcrete(CPU_OFFSET(regs[2]),
                                                 &funcptr, 4);
#elif defined(TARGET_I386)
            ok &= state->readCpuRegisterConcrete(CPU_OFFSET(regs[R_EAX]),
                                                 &dllname, 4);
            ok &= state->readCpuRegisterConcrete(CPU_OFFSET(regs[R_EBX]),
                                                 &funcname, 4);
            ok &= state->readCpuRegisterConcrete(CPU_OFFSET(regs[R_ECX]),
                                                 &funcptr, 4);
#else
            assert(false);
#endif

            if (!ok) {
                s2e()->getWarningsStream(state)
                    << "ERROR: symbolic argument was passed to s2e_op "
                       "rawmonitor loadimportdescriptor" << std::endl;
                break;
            }

            std::string dllnameStr;
            if(!state->readString(dllname, dllnameStr)) {
                s2e()->getWarningsStream(state)
                        << "Error reading dll name string from the guest" << std::endl;
                return;
            }

            std::string funcnameStr;
            if(!state->readString(funcname, funcnameStr)) {
                s2e()->getWarningsStream(state)
                        << "Error reading function name string from the guest" << std::endl;
                return;
            }

            s2e()->getMessagesStream() << "RawMonitor: Registering " << dllnameStr << " "
                    << funcnameStr << " @0x" << std::hex << funcptr << std::endl;

            m_imports[dllnameStr][funcnameStr] = funcptr;
            break;
        }

    default:
        s2e()->getWarningsStream() << "Invalid RawMonitor opcode 0x" << std::hex << opcode << std::dec << std::endl;
        break;
    }
}

void RawMonitor::loadModule(S2EExecutionState *state, const Cfg &c, bool skipIfDelay)
{
    ModuleDescriptor md;
    if (c.delayLoad && skipIfDelay) {
        return;
    }
    md.Name = c.name;
    md.NativeBase = c.nativebase;
    md.LoadBase = c.start;
    md.Size = c.size;
    //XXX: PID from state does not work for ARM
    md.Pid = c.kernelMode ? 0 : state->getPid();
    md.EntryPoint = c.entrypoint;

    s2e()->getDebugStream() << "RawMonitor loaded " << c.name << " " <<
            std::hex << "0x" << c.start << " 0x" << c.size << std::dec << std::endl;
    onModuleLoad.emit(state, md);
}

void RawMonitor::onTranslateInstructionStart(ExecutionSignal *signal,
                                                   S2EExecutionState *state,
                                                   TranslationBlock *tb,
                                                   uint64_t pc)
{
    CfgList::const_iterator it;

    for (it = m_cfg.begin(); it != m_cfg.end(); ++it) {
        const Cfg &c = *it;
        loadModule(state, c, true);
    }

    m_onTranslateInstruction.disconnect();
}


bool RawMonitor::getImports(S2EExecutionState *s, const ModuleDescriptor &desc, Imports &I)
{
    I = m_imports;
    return true;
}

bool RawMonitor::getExports(S2EExecutionState *s, const ModuleDescriptor &desc, Exports &E)
{
    return false;
}

bool RawMonitor::isKernelAddress(uint64_t pc) const
{
    return false;
}

uint64_t RawMonitor::getPid(S2EExecutionState *s, uint64_t pc)
{
    if (pc >= m_kernelStart) {
        return 0;
    }
    return s->getPid();
}
