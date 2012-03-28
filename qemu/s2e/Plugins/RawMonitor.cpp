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
        s2e()->getWarningsStream() << "You must specify " << cfgKey <<  ".name\n";
        return false;
    }

    cfg.size = s2e()->getConfig()->getInt(cfgKey + ".size", 0, &ok);
    if (!ok) {
        s2e()->getWarningsStream() << "You must specify " << cfgKey <<  ".size\n";
        return false;
    }

    cfg.start = s2e()->getConfig()->getInt(cfgKey + ".start", 0, &ok);
    if (!ok) {
        s2e()->getWarningsStream() << "You must specify " << cfgKey <<  ".start\n";
        return false;
    }

    cfg.nativebase = s2e()->getConfig()->getInt(cfgKey + ".nativebase", 0, &ok);
    if (!ok) {
        s2e()->getWarningsStream() << "You must specify " << cfgKey <<  ".nativebase\n";
        return false;
    }

    cfg.delayLoad = s2e()->getConfig()->getBool(cfgKey + ".delay", false, &ok);
    if (!ok) {
        s2e()->getWarningsStream() << "You must specify " << cfgKey <<  ".delay\n";
        return false;
    }

    cfg.kernelMode = s2e()->getConfig()->getBool(cfgKey + ".kernelmode", false, &ok);
    if (!ok) {
        s2e()->getWarningsStream() << "You must specify " << cfgKey <<  ".kernelmode\n";
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
    m_kernelStart = s2e()->getConfig()->getInt(getConfigKey() + ".kernelStart", 0xc0000000, &ok);
    if (!ok) {
        s2e()->getWarningsStream() << "You should specify " << getConfigKey() << ".kernelStart\n";
    }

    foreach2(it, Sections.begin(), Sections.end()) {
        if (*it == "kernelStart") {
            continue;
        }

        s2e()->getMessagesStream() << "Scanning section " << getConfigKey() << "." << *it << '\n';
        std::stringstream sk;
        sk << getConfigKey() << "." << *it;
        if (!initSection(sk.str(), *it)) {
            noErrors = false;
        }
    }

    if (!noErrors) {
        s2e()->getWarningsStream() << "Errors while scanning the RawMonitor sections\n";
        exit(-1);
    }

    m_onTranslateInstruction = s2e()->getCorePlugin()->onTranslateInstructionStart.connect(
        sigc::mem_fun(*this, &RawMonitor::onTranslateInstructionStart));

    s2e()->getCorePlugin()->onCustomInstruction.connect(
            sigc::mem_fun(*this, &RawMonitor::onCustomInstruction));

}

/**********************************************************/
/**********************************************************/
/**********************************************************/

void RawMonitor::opLoadConfiguredModule(S2EExecutionState *state)
{
    uint32_t rtloadbase, name, size;
    bool ok = true;

    ok &= state->readCpuRegisterConcrete(CPU_OFFSET(regs[R_EAX]),
                                         &name, 4);
    ok &= state->readCpuRegisterConcrete(CPU_OFFSET(regs[R_EBX]),
                                         &rtloadbase, 4);
    ok &= state->readCpuRegisterConcrete(CPU_OFFSET(regs[R_ECX]),
                                         &size, 4);

    if(!ok) {
        s2e()->getWarningsStream(state)
            << "ERROR: symbolic argument was passed to s2e_op "
               "rawmonitor loadmodule\n";
        return;
    }

    std::string nameStr;
    if(!state->readString(name, nameStr)) {
        s2e()->getWarningsStream(state)
                << "Error reading module name string from the guest\n";
        return;
    }

    //Look for the module in the config section and update its load address
    CfgList::iterator it;

    for (it = m_cfg.begin(); it != m_cfg.end(); ++it) {
        Cfg &c = *it;
        if (c.name == nameStr) {
            s2e()->getMessagesStream() << "RawMonitor: Registering " << nameStr << " "
                                          " @" << hexval(rtloadbase) << " size=" << hexval(size)  << '\n';
            c.start = rtloadbase;
            c.size = size;
            loadModule(state, c, false);
        }
    }
}

void RawMonitor::opLoadModule(S2EExecutionState *state)
{
    uint32_t pModuleConfig;
    bool ok = true;

    ok &= state->readCpuRegisterConcrete(CPU_OFFSET(regs[R_ECX]),
                                         &pModuleConfig, sizeof(pModuleConfig));
    if(!ok) {
        s2e()->getWarningsStream(state)
            << "RawMonitor: Could not read the module descriptor address from the guest\n";
        return;
    }

    OpcodeModuleConfig moduleConfig;
    ok &= state->readMemoryConcrete(pModuleConfig, &moduleConfig, sizeof(moduleConfig));
    if(!ok) {
        s2e()->getWarningsStream(state)
            << "RawMonitor: Could not read the module descriptor from the guest\n";
        return;
    }

    ModuleDescriptor moduleDescriptor;
    moduleDescriptor.NativeBase = moduleConfig.nativeBase;
    moduleDescriptor.LoadBase = moduleConfig.loadBase;
    moduleDescriptor.EntryPoint = moduleConfig.entryPoint;
    moduleDescriptor.Size = moduleConfig.size;

    if (!state->readString(moduleConfig.name, moduleDescriptor.Name)) {
        s2e()->getWarningsStream(state)
            << "RawMonitor: Could not read the module string\n";
        return;
    }

    moduleDescriptor.Pid = moduleConfig.kernelMode ? 0 : state->getPid();

    s2e()->getDebugStream() << "RawMonitor loaded " << moduleDescriptor.Name << " " <<
            hexval(moduleDescriptor.LoadBase) << " " << hexval(moduleDescriptor.Size) << "\n";

    onModuleLoad.emit(state, moduleDescriptor);
}

void RawMonitor::opCreateImportDescriptor(S2EExecutionState *state)
{
    uint32_t dllname, funcname, funcptr;
    bool ok = true;
    ok &= state->readCpuRegisterConcrete(CPU_OFFSET(regs[R_EAX]),
                                         &dllname, 4);
    ok &= state->readCpuRegisterConcrete(CPU_OFFSET(regs[R_EBX]),
                                         &funcname, 4);
    ok &= state->readCpuRegisterConcrete(CPU_OFFSET(regs[R_ECX]),
                                         &funcptr, 4);

    if (!ok) {
        s2e()->getWarningsStream(state)
            << "ERROR: symbolic argument was passed to s2e_op "
               "rawmonitor loadimportdescriptor\n";
        return;
    }

    std::string dllnameStr;
    if(!state->readString(dllname, dllnameStr)) {
        s2e()->getWarningsStream(state)
                << "Error reading dll name string from the guest\n";
        return;
    }

    std::string funcnameStr;
    if(!state->readString(funcname, funcnameStr)) {
        s2e()->getWarningsStream(state)
                << "Error reading function name string from the guest\n";
        return;
    }

    s2e()->getMessagesStream() << "RawMonitor: Registering " << dllnameStr << " "
                               << funcnameStr << " @" << hexval(funcptr) << '\n';

    m_imports[dllnameStr][funcnameStr] = funcptr;
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
    case 0: {
        //Module load
        //eax = pointer to module name
        //ebx = runtime load base
        //ecx = entry point
        opLoadConfiguredModule(state);
        break;
    }

    case 1: {
        //Specifying a new import descriptor
        opCreateImportDescriptor(state);
        break;
    }

    case 2: {
        //Load a non-configured module.
        //Pointer to OpcodeModuleConfig structure is passed in ecx.
        opLoadModule(state);
        break;
    }

    default:
        s2e()->getWarningsStream() << "Invalid RawMonitor opcode " << hexval(opcode) << '\n';
        break;
    }
}


/**********************************************************/
/**********************************************************/
/**********************************************************/

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
    md.Pid = c.kernelMode ? 0 : state->getPid();
    md.EntryPoint = c.entrypoint;

    s2e()->getDebugStream() << "RawMonitor loaded " << c.name << " " <<
            hexval(c.start) << ' ' << hexval(c.size) << '\n';
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
