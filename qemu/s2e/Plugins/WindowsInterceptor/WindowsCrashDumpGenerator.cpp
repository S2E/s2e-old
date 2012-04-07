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

extern "C" {
#include "config.h"
#include "qemu-common.h"
#include "cpu.h"
extern CPUX86State *env;
}


#include "WindowsCrashDumpGenerator.h"
#include <s2e/S2E.h>
#include <s2e/S2EExecutor.h>
#include <s2e/ConfigFile.h>
#include <s2e/Utils.h>
#include <s2e/Plugins/WindowsApi/Api.h>

#include <iostream>
#include <sstream>

namespace s2e {
namespace plugins {

using namespace s2e::windows;

S2E_DEFINE_PLUGIN(WindowsCrashDumpGenerator, "Generates WinDbg-compatible crash dumps",
                  "WindowsCrashDumpGenerator", "WindowsMonitor");

void WindowsCrashDumpGenerator::initialize()
{
    //Register the LUA API for crash dump generation
    Lunar<WindowsCrashDumpInvoker>::Register(s2e()->getConfig()->getState());

    m_monitor = static_cast<WindowsMonitor*>(s2e()->getPlugin("WindowsMonitor"));
}

void WindowsCrashDumpGenerator::generateDump(S2EExecutionState *state, const std::string &prefix)
{
    s2e::windows::CONTEXT32 context;
    if (!saveContext(state, context)) {
        s2e()->getDebugStream() << "Could not save BSOD context" << '\n';
        return;
    }
    context.Eip = state->readCpuState(offsetof(CPUX86State, eip), 8*sizeof(uint32_t));


    BugCheckDesc bugDesc;
    memset(&bugDesc, 0, sizeof(bugDesc));
    bugDesc.code = 0xDEADDEAD; //MANUALLY_INITIATED_CRASH1

    generateCrashDump(state, prefix, context, bugDesc);
}

void WindowsCrashDumpGenerator::generateDumpOnBsod(S2EExecutionState *state, const std::string &prefix)
{
    s2e::windows::CONTEXT32 context;
    if (!saveContext(state, context)) {
        s2e()->getDebugStream() << "Could not save BSOD context" << '\n';
        return;
    }

    BugCheckDesc bugDesc;
    WindowsApi::readConcreteParameter(state, 0, &bugDesc.code);
    WindowsApi::readConcreteParameter(state, 1, &bugDesc.param1);
    WindowsApi::readConcreteParameter(state, 2, &bugDesc.param2);
    WindowsApi::readConcreteParameter(state, 3, &bugDesc.param3);
    WindowsApi::readConcreteParameter(state, 4, &bugDesc.param4);

    generateCrashDump(state, prefix, context, bugDesc);
}

//Move this to executionstate
uint32_t WindowsCrashDumpGenerator::readAndConcretizeRegister(S2EExecutionState *state, unsigned offset)
{
    klee::ref<klee::Expr> reg = state->readCpuRegister(offset, klee::Expr::Int32);
    klee::ConstantExpr* ce;
    if ((ce = dyn_cast<klee::ConstantExpr>(reg))) {
        uint64_t v = ce->getZExtValue(64);
        return (uint32_t)v;
    }else {
        //Make it concrete
        uint32_t ch = s2e()->getExecutor()->toConstant(*state, reg,
                                "concretizing for crash dump")->getZExtValue(32);
        state->writeCpuRegisterConcrete(offset, &ch, sizeof(ch));
        return ch;
    }
}

bool WindowsCrashDumpGenerator::saveContext(S2EExecutionState *state, s2e::windows::CONTEXT32 &ctx)
{
    memset(&ctx, 0x0, sizeof(ctx));

    ctx.ContextFlags = CONTEXT_FULL;

    ctx.Eax = readAndConcretizeRegister(state, offsetof(CPUX86State, regs[R_EAX]));
    ctx.Ebx = readAndConcretizeRegister(state, offsetof(CPUX86State, regs[R_EBX]));
    ctx.Ecx = readAndConcretizeRegister(state, offsetof(CPUX86State, regs[R_ECX]));
    ctx.Edx = readAndConcretizeRegister(state, offsetof(CPUX86State, regs[R_EDX]));
    ctx.Esi = readAndConcretizeRegister(state, offsetof(CPUX86State, regs[R_ESI]));
    ctx.Edi = readAndConcretizeRegister(state, offsetof(CPUX86State, regs[R_EDI]));
    ctx.Esp = readAndConcretizeRegister(state, offsetof(CPUX86State, regs[R_ESP]));
    ctx.Ebp = readAndConcretizeRegister(state, offsetof(CPUX86State, regs[R_EBP]));

    ctx.Dr0 = state->readCpuState(offsetof(CPUX86State, dr[0]), 8*sizeof(uint32_t));
    ctx.Dr1 = state->readCpuState(offsetof(CPUX86State, dr[1]), 8*sizeof(uint32_t));
    ctx.Dr2 = state->readCpuState(offsetof(CPUX86State, dr[2]), 8*sizeof(uint32_t));
    ctx.Dr3 = state->readCpuState(offsetof(CPUX86State, dr[3]), 8*sizeof(uint32_t));
    ctx.Dr6 = state->readCpuState(offsetof(CPUX86State, dr[6]), 8*sizeof(uint32_t));
    ctx.Dr7 = state->readCpuState(offsetof(CPUX86State, dr[7]), 8*sizeof(uint32_t));

    ctx.SegDs = state->readCpuState(offsetof(CPUX86State, segs[R_DS].selector), 8*sizeof(uint32_t));
    ctx.SegEs = state->readCpuState(offsetof(CPUX86State, segs[R_ES].selector), 8*sizeof(uint32_t));
    ctx.SegFs = state->readCpuState(offsetof(CPUX86State, segs[R_FS].selector), 8*sizeof(uint32_t));
    ctx.SegGs = state->readCpuState(offsetof(CPUX86State, segs[R_GS].selector), 8*sizeof(uint32_t));

    ctx.SegCs = state->readCpuState(offsetof(CPUX86State, segs[R_CS].selector), 8*sizeof(uint32_t));
    ctx.SegSs = state->readCpuState(offsetof(CPUX86State, segs[R_SS].selector), 8*sizeof(uint32_t));


    ctx.EFlags = state->getFlags();

    //Take the return address of the bugcheck call
    uint64_t pc;
    state->getReturnAddress(&pc);
    ctx.Eip = (uint32_t)pc;
    //XXX: Save eflags
    return true;
}

//XXX: Use exceptions instead of gotos...
void WindowsCrashDumpGenerator::generateCrashDump(S2EExecutionState *state,
                                                  const std::string &prefix,
                                                  s2e::windows::CONTEXT32 &context,
                                                  const BugCheckDesc &bugdesc)
{
    uint32_t KprcbProcessContextOffset;

    uint32_t *blocks;
    uint8_t *rawhdr;
    DUMP_HEADER32 *hdr;
    bool ok;
    uint8_t ZeroPage[0x1000];
    uint8_t TempPage[0x1000];
    klee::ref<klee::Expr> OriginalContext[sizeof(CONTEXT32)];

    PHYSICAL_MEMORY_DESCRIPTOR *ppmd;

    unsigned PagesWritten, CurrentMemoryRun;

    std::stringstream filename;
    filename << prefix << state->getID() << ".dump";
    llvm::raw_ostream &d = *s2e()->openOutputFile(filename.str());

    rawhdr = new uint8_t[0x1000];
    hdr = (DUMP_HEADER32*)rawhdr;

    //Set all dwords in the header to the magic value
    for (unsigned i=0; i<0x1000; i+=4) {
        *(uint32_t*)(rawhdr + i) = DUMP_HDR_SIGNATURE;
    }

    //Init and write the header
    if (!initializeHeader(state, hdr, context, bugdesc)) {
        goto err1;
    }

    d.write((const char*)rawhdr, 0x1000);


    //Save the original context
    KprcbProcessContextOffset = m_monitor->getKpcrbAddress() + offsetof(KPRCB32, ProcessorState.ContextFrame);
    for (unsigned i=0; i<sizeof(CONTEXT32); ++i) {
        OriginalContext[i] = state->readMemory(KprcbProcessContextOffset + i, klee::Expr::Int8);
    }

    //Write the new one to the KPRCB
    //WinDBG also expects it in the KPCRB, which is null by default
    ok = state->writeMemoryConcrete(KprcbProcessContextOffset, &context, sizeof(CONTEXT32));
    if (!ok) {
        s2e()->getDebugStream() << "Could not write the context to KPRCB" << '\n';
        goto err1;
    }


    //Dump the physical memory
    blocks = (uint32_t*)hdr;
    ppmd = (PHYSICAL_MEMORY_DESCRIPTOR*) &blocks[ DH_PHYSICAL_MEMORY_BLOCK ];

    PagesWritten = 0;

    memset( ZeroPage, 0, 0x1000 );

    CurrentMemoryRun = 0;
    while( CurrentMemoryRun < ppmd->NumberOfRuns ) {
        if( ppmd->Run[CurrentMemoryRun].PageCount == DUMP_HDR_SIGNATURE || ppmd->Run[CurrentMemoryRun].BasePage == DUMP_HDR_SIGNATURE )
        {
            s2e()->getDebugStream() << "PHYSICAL_MEMORY_DESCRIPTOR corrupted." << '\n';
            break;
        }

        s2e()->getDebugStream() << "Processing run " << CurrentMemoryRun << '\n';


        uint32_t ProcessedPagesInCurrentRun = 0;
        while( ProcessedPagesInCurrentRun < ppmd->Run[CurrentMemoryRun].PageCount ) {
            uint32_t physAddr = (ppmd->Run[CurrentMemoryRun].BasePage + ProcessedPagesInCurrentRun) * 0x1000;

            //s2e()->getDebugStream() << "Processing page " << std::dec << ProcessedPagesInCurrentRun
            //        << "(addr=0x" << std::hex << physAddr << '\n';

            memset(TempPage, 0xDA, sizeof(TempPage));
            for (uint32_t i=0; i<0x1000; ++i) {
                klee::ref<klee::Expr> v = state->readMemory(physAddr+i, klee::Expr::Int8, S2EExecutionState::PhysicalAddress);
                if (v.isNull() || !isa<klee::ConstantExpr>(v)) {
                    //Make it concrete
                    TempPage[i] = s2e()->getExecutor()->toConstant(*state, v,
                                            "concretizing memory for crash dump")->getZExtValue(8);                    
                }else {
                    TempPage[i] = (uint8_t)cast<klee::ConstantExpr>(v)->getZExtValue(8);
                }
            }

            d.write((const char*)TempPage, 0x1000);

            PagesWritten ++;
            ProcessedPagesInCurrentRun ++;
        }
        CurrentMemoryRun ++;
    }

    //Restore the original context
    for (unsigned i=0; i<sizeof(CONTEXT32); ++i) {
        state->writeMemory(KprcbProcessContextOffset + i, OriginalContext[i]);
    }

    err1:
    delete [] rawhdr;
}

bool WindowsCrashDumpGenerator::initializeHeader(S2EExecutionState *state, DUMP_HEADER32 *hdr,
                                                 const s2e::windows::CONTEXT32 &ctx,
                                                 const BugCheckDesc &bugdesc)
{
    uint32_t *blocks = NULL;

    bool ok;

    hdr->ValidDump = DUMP_HDR_DUMPSIGNATURE;
    hdr->MajorVersion = m_monitor->isCheckedBuild() ? 0xC : 0xF; //Free build (0xC for checked)
    hdr->MinorVersion = m_monitor->getBuildNumber();
    hdr->DirectoryTableBase = state->getPid();

    //Fetch KdDebuggerDataBlock
    //XXX: May break with windows versions
    uint32_t pKdDebuggerDataBlock = m_monitor->GetKdDebuggerDataBlock();
    hdr->KdDebuggerDataBlock = pKdDebuggerDataBlock;


    //Initialize bugcheck codes
    hdr->BugCheckCode = bugdesc.code;
    hdr->BugCheckParameter1 = bugdesc.param1;
    hdr->BugCheckParameter2 = bugdesc.param2;
    hdr->BugCheckParameter3 = bugdesc.param3;
    hdr->BugCheckParameter4 = bugdesc.param4;

    hdr->MachineImageType = 0x14c;
    hdr->NumberProcessors = 1;

    uint32_t cr4 = state->readCpuState(offsetof(CPUX86State, cr[4]), 8*sizeof(target_ulong));
    hdr->PaeEnabled = (cr4 & PAE_ENABLED) ? 1 : 0;

    // Check KdDebuggerDataBlock
    KD_DEBUGGER_DATA_BLOCK32 KdDebuggerDataBlock;
    ok = state->readMemoryConcrete(hdr->KdDebuggerDataBlock, &KdDebuggerDataBlock, sizeof(KdDebuggerDataBlock));
    if (!ok) {
        s2e()->getDebugStream() << "Could not read KD_DEBUGGER_DATA_BLOCK32" << '\n';
        return false;
    }

    if(KdDebuggerDataBlock.ValidBlock != DUMP_KDBG_SIGNATURE || KdDebuggerDataBlock.Size != sizeof(KdDebuggerDataBlock) )
    {
        // Invalid debugger data block
        s2e()->getDebugStream() << "KD_DEBUGGER_DATA_BLOCK32 is invalid" << '\n';
        return false;
    }

    hdr->PfnDataBase = KdDebuggerDataBlock.MmPfnDatabase.VirtualAddress;
    hdr->PsLoadedModuleList =  KdDebuggerDataBlock.PsLoadedModuleList.VirtualAddress;
    hdr->PsActiveProcessHead = KdDebuggerDataBlock.PsActiveProcessHead.VirtualAddress;

    //Get physical memory descriptor
    uint32_t pMmPhysicalMemoryBlock;
    ok = state->readMemoryConcrete(KdDebuggerDataBlock.MmPhysicalMemoryBlock.VirtualAddress,
                                   &pMmPhysicalMemoryBlock, sizeof(pMmPhysicalMemoryBlock));
    if (!ok) {
        s2e()->getDebugStream() << "Could not read pMmPhysicalMemoryBlock" << '\n';
        return false;
    }

    //Determine the number of runs
    uint32_t RunCount;
    ok = state->readMemoryConcrete(pMmPhysicalMemoryBlock,
                                   &RunCount, sizeof(RunCount));

    if (!ok) {
        s2e()->getDebugStream() << "Could not read number of runs" << '\n';
        return false;
    }

    //Allocate enough memory for reading the whole structure
    size_t SizeOfMemoryDescriptor;
    if (RunCount == DUMP_HDR_SIGNATURE) {
        SizeOfMemoryDescriptor = sizeof(PHYSICAL_MEMORY_DESCRIPTOR);
    } else {
        SizeOfMemoryDescriptor = sizeof(PHYSICAL_MEMORY_DESCRIPTOR) - sizeof(PHYSICAL_MEMORY_RUN) +
        sizeof(PHYSICAL_MEMORY_RUN)*RunCount;
    }

    //XXX: is there a more beautiful way of doing this?
    PHYSICAL_MEMORY_DESCRIPTOR *MmPhysicalMemoryBlock =
            (PHYSICAL_MEMORY_DESCRIPTOR *)new uint8_t[SizeOfMemoryDescriptor];

    ok = state->readMemoryConcrete(pMmPhysicalMemoryBlock,
                                   MmPhysicalMemoryBlock, SizeOfMemoryDescriptor);
    if (!ok) {
        s2e()->getDebugStream() << "Could not read PHYSICAL_MEMORY_DESCRIPTOR" << '\n';
        delete [] MmPhysicalMemoryBlock;
        return false;
    }

    blocks = (uint32_t*)(uintptr_t)hdr;

    memcpy(&blocks[ DH_PHYSICAL_MEMORY_BLOCK ], MmPhysicalMemoryBlock, SizeOfMemoryDescriptor);

    // Initialize dump type & size
    blocks[ DH_DUMP_TYPE ] = DUMP_TYPE_COMPLETE;
    *((uint64_t*)&blocks[DH_REQUIRED_DUMP_SPACE]) = ( MmPhysicalMemoryBlock->NumberOfPages << 12 ) + 0x1000;
    delete [] MmPhysicalMemoryBlock;


    s2e()->getDebugStream() << "Writing " << sizeof(ctx) << " bytes of DH_CONTEXT_RECORD" << '\n';
    memcpy(&blocks[ DH_CONTEXT_RECORD ],
                                    &ctx,
                                    sizeof(ctx)
                                    );

    s2e()->getDebugStream() << "Writing " << sizeof(ctx) << " bytes of DH_CONTEXT_RECORD" << '\n';
    memcpy(&blocks[ DH_CONTEXT_RECORD ],
                                    &ctx,
                                    sizeof(ctx)
                                    );

    s2e::windows::EXCEPTION_RECORD32 exception;
    memset(&exception, 0, sizeof(exception));
    exception.ExceptionCode = STATUS_BREAKPOINT;
    exception.ExceptionFlags = EXCEPTION_NONCONTINUABLE;
    exception.ExceptionRecord = 0;
    exception.ExceptionAddress = ctx.Eip;
    exception.NumberParameters = 0;

    s2e()->getDebugStream() << "Writing " << sizeof(exception) << " bytes of DH_EXCEPTION_RECORD" << '\n';
    //Windows does not store exception parameters in the header
    memcpy(&blocks[ DH_EXCEPTION_RECORD ],
                                    &exception,
                                    sizeof(exception) - sizeof(uint32_t)*EXCEPTION_MAXIMUM_PARAMETERS
                                    );

    //Filling in the rest...
    hdr->SecondaryDataState = 0;
    hdr->ProductType = 0x1;
    hdr->SuiteMask = 0x110;

    return true;

}

const char WindowsCrashDumpInvoker::className[] = "WindowsCrashDumpInvoker";

Lunar<WindowsCrashDumpInvoker>::RegType WindowsCrashDumpInvoker::methods[] = {
  LUNAR_DECLARE_METHOD(WindowsCrashDumpInvoker, generateCrashDump),
  {0,0}
};


WindowsCrashDumpInvoker::WindowsCrashDumpInvoker(WindowsCrashDumpGenerator *plg)
{
    m_plugin = plg;
}

WindowsCrashDumpInvoker::WindowsCrashDumpInvoker(lua_State *lua)
{
    m_plugin = static_cast<WindowsCrashDumpGenerator*>(g_s2e->getPlugin("WindowsCrashDumpGenerator"));
}

WindowsCrashDumpInvoker::~WindowsCrashDumpInvoker()
{

}

int WindowsCrashDumpInvoker::generateCrashDump(lua_State *L)
{
    llvm::raw_ostream &os = g_s2e->getDebugStream();

    if (!lua_isstring(L, 1)) {
        os << "First argument to " << __FUNCTION__ << " must be the prefix of the crash dump" << '\n';
        return 0;
    }

    std::string prefix = luaL_checkstring(L, 1);

    S2EExecutionState *state = g_s2e_state;
    int stateId = g_s2e_state->getID();
    if (lua_isnumber(L, 2)) {
        stateId = lua_tointeger(L, 2);
        state = NULL;

        //Fetch the right state
        //XXX: Avoid linear search
        const std::set<klee::ExecutionState*> &states = g_s2e->getExecutor()->getStates();
        foreach2(it, states.begin(), states.end()) {
            S2EExecutionState *ss = static_cast<S2EExecutionState*>(*it);
            if (ss->getID() == stateId) {
                state = ss;
                break;
            }
        }
    }

    if (state == NULL) {
        os << "State with id " << stateId << " does not exist" << '\n';
        return 0;
    }

    if (!m_plugin) {
        os << "Please enable the WindowsCrashDumpGenerator plugin in your configuration file" << '\n';
        return 0;
    }

    m_plugin->generateDump(state, prefix);

    return 0;
}

} // namespace plugins
} // namespace s2e
