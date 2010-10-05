extern "C" {
#include "config.h"
#include "qemu-common.h"
#include "cpu.h"
}


#include <s2e/S2E.h>
#include <s2e/S2EExecutor.h>
#include "BlueScreenInterceptor.h"
#include <s2e/Plugins/ModuleExecutionDetector.h>
#include <s2e/Plugins/WindowsApi/Api.h>

#include <iomanip>
#include <sstream>

namespace s2e {
namespace plugins {

S2E_DEFINE_PLUGIN(BlueScreenInterceptor, "Intercepts Windows blue screens of death and generated bug reports",
                  "BlueScreenInterceptor", "WindowsMonitor");

void BlueScreenInterceptor::initialize()
{
    m_monitor = (WindowsMonitor*)s2e()->getPlugin("WindowsMonitor");
    assert(m_monitor);

    s2e()->getCorePlugin()->onTranslateBlockStart.connect(
            sigc::mem_fun(*this,
                    &BlueScreenInterceptor::onTranslateBlockStart));
}

void BlueScreenInterceptor::onTranslateBlockStart(
    ExecutionSignal *signal,
    S2EExecutionState *state,
    TranslationBlock *tb,
    uint64_t pc)
{
    if (!m_monitor->CheckPanic(pc)) {
        return;
    }

    signal->connect(sigc::mem_fun(*this,
        &BlueScreenInterceptor::onBsod));
}

void BlueScreenInterceptor::onBsod(
        S2EExecutionState *state, uint64_t pc)
{
    std::ostream &os = s2e()->getMessagesStream(state);

    os << "Killing state "  << state->getID() <<
            " because of BSOD " << std::endl;

    ModuleExecutionDetector *m_exec = (ModuleExecutionDetector*)s2e()->getPlugin("ModuleExecutionDetector");

    dispatchErrorCodes(state);

    if (m_exec) {
        m_exec->dumpMemory(state, os, state->getSp(), 512);
    }else {
        state->dumpStack(512);
    }

    //generateCrashDump(state);

    s2e()->getExecutor()->terminateStateEarly(*state, "Killing because of BSOD");
}

void BlueScreenInterceptor::dumpCriticalObjectTermination(S2EExecutionState *state)
{
    uint32_t terminatingObjectType;
    uint32_t terminatingObject;
    uint32_t processImageName;
    uint32_t message;

    bool ok = true;
    ok &= WindowsApi::readConcreteParameter(state, 1, &terminatingObjectType);
    ok &= WindowsApi::readConcreteParameter(state, 2, &terminatingObject);
    ok &= WindowsApi::readConcreteParameter(state, 3, &processImageName);
    ok &= WindowsApi::readConcreteParameter(state, 4, &message);

    if (!ok) {
        s2e()->getDebugStream() << "Could not read BSOD parameters" << std::endl;
    }

    std::string strMessage, strImage;
    ok &= state->readString(message, strMessage, 256);
    ok &= state->readString(processImageName, strImage, 256);

    s2e()->getDebugStream(state) <<
            "CRITICAL_OBJECT_TERMINATION" << std::endl <<
            "ImageName: " << strImage << std::endl <<
            "Message:   " << strMessage << std::endl;
}

void BlueScreenInterceptor::dispatchErrorCodes(S2EExecutionState *state)
{
    uint32_t errorCode;

    WindowsApi::readConcreteParameter(state, 0, &errorCode);
    switch(errorCode) {
    case CRITICAL_OBJECT_TERMINATION:
        dumpCriticalObjectTermination(state);
        break;

    default:
        s2e()->getDebugStream() << "Unknown BSOD code " << errorCode << std::endl;
        break;
    }
}

//Move this to executionstate
uint32_t BlueScreenInterceptor::readAndConcretizeRegister(S2EExecutionState *state, unsigned offset)
{
    klee::ref<klee::Expr> reg = state->readCpuRegister(offset, klee::Expr::Int32);
    klee::ConstantExpr* ce;
    if ((ce = dyn_cast<klee::ConstantExpr>(reg))) {
        uint64_t v = ce->getZExtValue(64);
        return (uint32_t)v;
    }else {
        //Make it concrete
        //uint32_t ch = s2e()->getExecutor()->toConstant(*state, reg,
        //                        "concretizing for crash dump")->getZExtValue(32);
        //state->writeCpuRegisterConcrete(offset, &ch, sizeof(ch));
        //return ch;
        //XXX: properly fix this.
        return 0xBADCAFE;
    }
}

bool BlueScreenInterceptor::saveContext(S2EExecutionState *state, s2e::windows::CONTEXT32 &ctx)
{
    memset(&ctx, 0, sizeof(ctx));

    ctx.Eax = readAndConcretizeRegister(state, offsetof(CPUState, regs[R_EAX]));
    ctx.Ebx = readAndConcretizeRegister(state, offsetof(CPUState, regs[R_EBX]));
    ctx.Ecx = readAndConcretizeRegister(state, offsetof(CPUState, regs[R_ECX]));
    ctx.Edx = readAndConcretizeRegister(state, offsetof(CPUState, regs[R_EDX]));
    ctx.Esi = readAndConcretizeRegister(state, offsetof(CPUState, regs[R_ESI]));
    ctx.Edi = readAndConcretizeRegister(state, offsetof(CPUState, regs[R_EDI]));
    ctx.Esp = readAndConcretizeRegister(state, offsetof(CPUState, regs[R_ESP]));
    ctx.Ebp = readAndConcretizeRegister(state, offsetof(CPUState, regs[R_EBP]));

    ctx.Dr0 = state->readCpuState(offsetof(CPUState, dr[0]), 8*sizeof(uint32_t));
    ctx.Dr1 = state->readCpuState(offsetof(CPUState, dr[1]), 8*sizeof(uint32_t));
    ctx.Dr2 = state->readCpuState(offsetof(CPUState, dr[2]), 8*sizeof(uint32_t));
    ctx.Dr3 = state->readCpuState(offsetof(CPUState, dr[3]), 8*sizeof(uint32_t));
    ctx.Dr6 = state->readCpuState(offsetof(CPUState, dr[6]), 8*sizeof(uint32_t));
    ctx.Dr7 = state->readCpuState(offsetof(CPUState, dr[7]), 8*sizeof(uint32_t));

    ctx.SegDs = state->readCpuState(offsetof(CPUState, segs[R_DS].selector), 8*sizeof(uint32_t));
    ctx.SegEs = state->readCpuState(offsetof(CPUState, segs[R_ES].selector), 8*sizeof(uint32_t));
    ctx.SegFs = state->readCpuState(offsetof(CPUState, segs[R_FS].selector), 8*sizeof(uint32_t));
    ctx.SegGs = state->readCpuState(offsetof(CPUState, segs[R_GS].selector), 8*sizeof(uint32_t));

    ctx.SegCs = state->readCpuState(offsetof(CPUState, segs[R_CS].selector), 8*sizeof(uint32_t));
    ctx.SegSs = state->readCpuState(offsetof(CPUState, segs[R_SS].selector), 8*sizeof(uint32_t));

    ctx.Eip = state->readCpuState(offsetof(CPUState, eip), 8*sizeof(uint32_t));

    //XXX: Save eflags
    return true;
}

void BlueScreenInterceptor::generateCrashDump(S2EExecutionState *state)
{
    uint32_t *blocks;
    uint8_t *rawhdr;
    DUMP_HEADER32 *hdr;
    uint8_t ZeroPage[0x1000];
    uint8_t TempPage[0x1000];

    PHYSICAL_MEMORY_DESCRIPTOR *ppmd;

    unsigned PagesWritten, PreviousPercent, TotalPages,
    CurrentMemoryRun;

    std::stringstream filename;
    filename << state->getID() << ".dump";
    std::ostream &d = *s2e()->openOutputFile(filename.str());

    rawhdr = new uint8_t[0x1000];
    hdr = (DUMP_HEADER32*)rawhdr;

    if (!initializeHeader(state, hdr)) {
        goto err1;
    }

    d.write((const char*)rawhdr, 0x1000);

    blocks = (uint32_t*)hdr;
    ppmd = (PHYSICAL_MEMORY_DESCRIPTOR*) &blocks[ DH_PHYSICAL_MEMORY_BLOCK ];

    PagesWritten = 0;
    PreviousPercent = 0;
    TotalPages = ppmd->NumberOfPages;

    memset( ZeroPage, 0, 0x1000 );

    while( CurrentMemoryRun < ppmd->NumberOfRuns ) {
        if( ppmd->Run[CurrentMemoryRun].PageCount == 'EGAP' || ppmd->Run[CurrentMemoryRun].BasePage == 'EGAP' )
        {
            s2e()->getDebugStream() << "PHYSICAL_MEMORY_DESCRIPTOR corrupted." << std::endl;
            break;
        }


        uint32_t ProcessedPagesInCurrentRun = 0;
        while( ProcessedPagesInCurrentRun < ppmd->Run[CurrentMemoryRun].PageCount ) {
            uint32_t physAddr = ppmd->Run[CurrentMemoryRun].BasePage + ProcessedPagesInCurrentRun;

            memset(TempPage, 0, sizeof(TempPage));
            for (uint32_t i=0; i<0x1000; ++i) {
                klee::ref<klee::Expr> v = state->readMemory(physAddr, klee::Expr::Int8, S2EExecutionState::PhysicalAddress);
                if (v.isNull() || !isa<klee::ConstantExpr>(v)) {
                    //XXX fix this
                    continue;
                }
                TempPage[i] = (uint8_t)cast<klee::ConstantExpr>(v)->getZExtValue(8);
            }

            d.write((const char*)TempPage, 0x1000);

            PagesWritten ++;
            ProcessedPagesInCurrentRun ++;
        }
        CurrentMemoryRun ++;
    }


    err1:
    delete [] rawhdr;
}

bool BlueScreenInterceptor::initializeHeader(S2EExecutionState *state, DUMP_HEADER32 *hdr)
{
    uint32_t *blocks = NULL;

    bool ok;
    s2e::windows::KPRCB32 kprcb;
    ok = state->readMemoryConcrete(KPRCB_OFFSET, &kprcb, sizeof(kprcb));
    if (!ok) {
        s2e()->getDebugStream() << "Could not read KPRCB" << std::endl;
        return false;
    }

    uint8_t *rawhdr = (uint8_t*)hdr;
    memset(rawhdr, 'EGAP', 0x1000);

    hdr->ValidDump = 'PMUD';
    hdr->MajorVersion = kprcb.MajorVersion;
    hdr->MinorVersion = kprcb.MinorVersion;
    hdr->DirectoryTableBase = state->getPid();

    //Fetch KdDebuggerDataBlock
    //XXX: May break with windows versions
    uint32_t pKdDebuggerDataBlock = 0x804d7000 - 0x400000 + 0x00475DE0;
    hdr->KdDebuggerDataBlock = pKdDebuggerDataBlock;


    //Initiliaze bugcheck codes
    WindowsApi::readConcreteParameter(state, 0, &hdr->BugCheckCode);
    WindowsApi::readConcreteParameter(state, 1, &hdr->BugCheckParameter1);
    WindowsApi::readConcreteParameter(state, 2, &hdr->BugCheckParameter2);
    WindowsApi::readConcreteParameter(state, 3, &hdr->BugCheckParameter3);
    WindowsApi::readConcreteParameter(state, 4, &hdr->BugCheckParameter4);

    hdr->MachineImageType = 0x14c;
    hdr->NumberProcessors = 1;

    uint32_t cr4 = state->readCpuState(offsetof(CPUX86State, cr[4]), 8*sizeof(target_ulong));
    hdr->PaeEnabled = (cr4 & PAE_ENABLED) ? 1 : 0;

    // Check KdDebuggerDataBlock
    KD_DEBUGGER_DATA_BLOCK32 KdDebuggerDataBlock;
    ok = state->readMemoryConcrete(hdr->KdDebuggerDataBlock, &KdDebuggerDataBlock, sizeof(KdDebuggerDataBlock));
    if (!ok) {
        s2e()->getDebugStream() << "Could not read KD_DEBUGGER_DATA_BLOCK32" << std::endl;
        return false;
    }

    if(KdDebuggerDataBlock.ValidBlock != 'GBDK' || KdDebuggerDataBlock.Size != sizeof(KdDebuggerDataBlock) )
    {
        // Invalid debugger data block
        s2e()->getDebugStream() << "KD_DEBUGGER_DATA_BLOCK32 is invalid" << std::endl;
        return false;
    }

    hdr->PfnDataBase = KdDebuggerDataBlock.MmPfnDatabase.VirtualAddress;
    hdr->PsLoadedModuleList =  KdDebuggerDataBlock.PsLoadedModuleList.VirtualAddress;
    hdr->PsActiveProcessHead = KdDebuggerDataBlock.PsActiveProcessHead.VirtualAddress;

    //Get physical memory descriptor
    PHYSICAL_MEMORY_DESCRIPTOR MmPhysicalMemoryBlock;
    ok = state->readMemoryConcrete(KdDebuggerDataBlock.MmPhysicalMemoryBlock.VirtualAddress,
                                   &MmPhysicalMemoryBlock, sizeof(MmPhysicalMemoryBlock));
    if (!ok) {
        s2e()->getDebugStream() << "Could not read PHYSICAL_MEMORY_DESCRIPTOR" << std::endl;
        return false;
    }

    blocks = (uint32_t*)(uintptr_t)rawhdr;

    if( MmPhysicalMemoryBlock.NumberOfRuns == 'EGAP' ) {
        memcpy(	&blocks[ DH_PHYSICAL_MEMORY_BLOCK ],
                        &MmPhysicalMemoryBlock,
                        sizeof(PHYSICAL_MEMORY_DESCRIPTOR)
                        );
    } else {
        memcpy(	&blocks[ DH_PHYSICAL_MEMORY_BLOCK ],
                        &MmPhysicalMemoryBlock,
                        sizeof(PHYSICAL_MEMORY_DESCRIPTOR) - sizeof(PHYSICAL_MEMORY_RUN) +
                        sizeof(PHYSICAL_MEMORY_RUN)*MmPhysicalMemoryBlock.NumberOfRuns
                        );
    }


    s2e::windows::CONTEXT32 ctx;
    saveContext(state, ctx);
    memcpy(&blocks[ DH_CONTEXT_RECORD ],
                                    &ctx,
                                    sizeof(ctx)
                                    );

    s2e::windows::EXCEPTION_RECORD32 exception;
    exception.ExceptionCode = STATUS_BREAKPOINT;
    exception.ExceptionFlags = EXCEPTION_NONCONTINUABLE;
    exception.ExceptionRecord = 0;
    exception.ExceptionAddress = ctx.Eip;
    exception.NumberParameters = 0;

    memcpy(&blocks[ DH_EXCEPTION_RECORD ],
                                    &exception,
                                    sizeof(exception)
                                    );

    // Initialize dump type & size
    blocks[ DH_DUMP_TYPE ] = DUMP_TYPE_COMPLETE;
    *((uint64_t*)&blocks[DH_REQUIRED_DUMP_SPACE]) = ( MmPhysicalMemoryBlock.NumberOfPages << 12 ) + 0x1000;

    return true;

}

}
}
