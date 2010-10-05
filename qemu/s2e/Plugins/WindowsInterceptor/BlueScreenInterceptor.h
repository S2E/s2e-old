#ifndef S2E_PLUGINS_BSOD_H
#define S2E_PLUGINS_BSOD_H

#include <s2e/Plugin.h>
#include <s2e/Plugins/CorePlugin.h>
#include <s2e/S2EExecutionState.h>
#include "WindowsMonitor.h"
#include "WindowsImage.h"

namespace s2e {
namespace plugins {

//Below are data types used to generate Windows kernel crash dumps
//Most of the stuff is taken from http://www.wasm.ru/print.php?article=dmp_format_en

struct DUMP_HEADER32 {
/* 00 */    uint32_t Signature;
/* 04 */    uint32_t ValidDump;
/* 08 */    uint32_t MajorVersion;
/* 0c */    uint32_t MinorVersion;
/* 10 */    uint32_t DirectoryTableBase;
/* 14 */    uint32_t PfnDataBase;
/* 18 */    uint32_t PsLoadedModuleList; //PLIST_ENTRY
/* 1c */    uint32_t PsActiveProcessHead; //PLIST_ENTRY
/* 20 */    uint32_t MachineImageType;
/* 24 */    uint32_t NumberProcessors;
/* 28 */    uint32_t BugCheckCode;
/* 2c */    uint32_t BugCheckParameter1;
/* 30 */    uint32_t BugCheckParameter2;
/* 34 */    uint32_t BugCheckParameter3;
/* 38 */    uint32_t BugCheckParameter4;
/* 3c */    uint8_t  VersionUser[32];
#if 0
/* 40 */    uint32_t Spare1;
/* 44 */    uint32_t Spare2;
/* 48 */    uint32_t Unknown1;
/* 4c */    uint32_t Unknown2;
/* 50 */    uint32_t Unknown3;
/* 54 */    uint32_t Unknown4;
/* 58 */    uint32_t Unknown5;
#endif
/* 5c */    uint8_t PaeEnabled;
            uint8_t Reserved3[3];
/* 60 */    uint32_t KdDebuggerDataBlock; //uint32_t
}__attribute__((packed));

// Data Blocks
#define DH_PHYSICAL_MEMORY_BLOCK        25
#define DH_CONTEXT_RECORD               200
#define DH_EXCEPTION_RECORD             500
#define DH_DUMP_TYPE			994
#define	DH_REQUIRED_DUMP_SPACE		1000
#define DH_SUMMARY_DUMP_RECORD		1024

// Dump types
#define DUMP_TYPE_TRIAGE		4
#define DUMP_TYPE_SUMMARY		2
#define DUMP_TYPE_COMPLETE		1

// Triage dump header
struct TRIAGE_DUMP_HEADER32 {
    uint32_t	ServicePackBuild;	// 00
    uint32_t	SizeOfDump;			// 04
    uint32_t	ValidOffset;		// 08
    uint32_t	ContextOffset;		// 0c
    uint32_t	ExceptionOffset;	// 10
    uint32_t	MmOffset;			// 14
    uint32_t	UnloadedDriversOffset; // 18
    uint32_t	PrcbOffset;			// 1c
    uint32_t	ProcessOffset;		// 20
    uint32_t	ThreadOffset;		// 24
    uint32_t	Unknown1;			// 28
    uint32_t	Unknown2;			// 2c
    uint32_t	DriverListOffset;	// 30
    uint32_t	DriverCount;		// 34
    uint32_t	TriageOptions;		// 44
};
// size 1ah *4

// Kernel summary dump header
struct SUMMARY_DUMP_HEADER {
        uint32_t	Unknown1;			// 00
        uint32_t	ValidDump;			// 04
        uint32_t	Unknown2;			// 08
        uint32_t	HeaderSize;			// 0c
        uint32_t	BitmapSize;			// 10
        uint32_t	Pages;				// 14
        uint32_t	Unknown3;			// 18
        uint32_t	Unknown4;			// 1c

};
// size 20h

// Page frame number
typedef uint32_t PFN_NUMBER;

struct PHYSICAL_MEMORY_RUN {
    PFN_NUMBER BasePage;
    PFN_NUMBER PageCount;
};

struct PHYSICAL_MEMORY_DESCRIPTOR {
    uint32_t NumberOfRuns;
    PFN_NUMBER NumberOfPages;
    PHYSICAL_MEMORY_RUN Run[1];
};

// Bitmap
#define RtlCheckBit(BMH,BP) ((((BMH)->Buffer[(BP) / 32]) >> ((BP) % 32)) & 0x1)

template <class T>
struct PFUNC {
        T  VirtualAddress;
        uint32_t  ZeroField;
};

struct KD_DEBUGGER_DATA_BLOCK32 {
        uint32_t  Unknown1[4];
        uint32_t  ValidBlock; // 'GBDK'
        uint32_t  Size; // 0x290
        PFUNC<uint32_t>  _imp__VidInitialize;
        PFUNC<uint32_t>  RtlpBreakWithStatusInstruction;
        uint32_t  Unknown2[4];
        PFUNC<uint32_t>  KiCallUserMode;
        uint32_t  Unknown3[2];
        PFUNC<uint32_t>  PsLoadedModuleList;
        PFUNC<uint32_t>  PsActiveProcessHead;
        PFUNC<uint32_t>  PspCidTable;
        PFUNC<uint32_t>  ExpSystemResourcesList;
        PFUNC<uint32_t>  ExpPagedPoolDescriptor;
        PFUNC<uint32_t>  ExpNumberOfPagedPools;
        PFUNC<uint32_t>  KeTimeIncrement;
        PFUNC<uint32_t>  KeBugCheckCallbackListHead;
        PFUNC<uint32_t>  KiBugCheckData;
        PFUNC<uint32_t>  IopErrorLogListHead;
        PFUNC<uint32_t>  ObpRootDirectoryObject;
        PFUNC<uint32_t>  ObpTypeObjectType;
        PFUNC<uint32_t>  MmSystemCacheStart;
        PFUNC<uint32_t>  MmSystemCacheEnd;
        PFUNC<uint32_t>  MmSystemCacheWs;
        PFUNC<uint32_t>  MmPfnDatabase;
        PFUNC<uint32_t>  MmSystemPtesStart;
        PFUNC<uint32_t>  MmSystemPtesEnd;
        PFUNC<uint32_t>  MmSubsectionBase;
        PFUNC<uint32_t>  MmNumberOfPagingFiles;
        PFUNC<uint32_t>  MmLowestPhysicalPage;
        PFUNC<uint32_t>  MmHighestPhysicalPage;
        PFUNC<uint32_t>  MmNumberOfPhysicalPages;
        PFUNC<uint32_t>  MmMaximumNonPagedPoolInBytes;
        PFUNC<uint32_t>  MmNonPagedSystemStart;
        PFUNC<uint32_t>  MmNonPagedPoolStart;
        PFUNC<uint32_t>  MmNonPagedPoolEnd;
        PFUNC<uint32_t>  MmPagedPoolStart;
        PFUNC<uint32_t>  MmPagedPoolEnd;
        PFUNC<uint32_t>  MmPagedPoolInfo;
        PFUNC<uint32_t>  Unknown4;
        PFUNC<uint32_t>  MmSizeOfPagedPoolInBytes;
        PFUNC<uint32_t>  MmTotalCommitLimit;
        PFUNC<uint32_t>  MmTotalCommittedPages;
        PFUNC<uint32_t>  MmSharedCommit;
        PFUNC<uint32_t>  MmDriverCommit;
        PFUNC<uint32_t>  MmProcessCommit;
        PFUNC<uint32_t>  MmPagedPoolCommit;
        PFUNC<uint32_t>  Unknown5;
        PFUNC<uint32_t>  MmZeroedPageListHead;
        PFUNC<uint32_t>  MmFreePageListHead;
        PFUNC<uint32_t>  MmStandbyPageListHead;
        PFUNC<uint32_t>  MmModifiedPageListHead;
        PFUNC<uint32_t>  MmModifiedNoWritePageListHead;
        PFUNC<uint32_t>  MmAvailablePages;
        PFUNC<uint32_t>  MmResidentAvailablePages;
        PFUNC<uint32_t>  PoolTrackTable;
        PFUNC<uint32_t>  NonPagedPoolDescriptor;
        PFUNC<uint32_t>  MmHighestUserAddress;
        PFUNC<uint32_t>  MmSystemRangeStart;
        PFUNC<uint32_t>  MmUserProbeAddress;
        PFUNC<uint32_t>  KdPrintCircularBuffer;
        PFUNC<uint32_t>  KdPrintWritePointer;
        PFUNC<uint32_t>  KdPrintWritePointer2;
        PFUNC<uint32_t>  KdPrintRolloverCount;
        PFUNC<uint32_t>  MmLoadedUserImageList;
        PFUNC<uint32_t>  NtBuildLab;
        PFUNC<uint32_t>  Unknown6;
        PFUNC<uint32_t>  KiProcessorBlock;
        PFUNC<uint32_t>  MmUnloadedDrivers;
        PFUNC<uint32_t>  MmLastUnloadedDriver;
        PFUNC<uint32_t>  MmTriageActionTaken;
        PFUNC<uint32_t>  MmSpecialPoolTag;
        PFUNC<uint32_t>  KernelVerifier;
        PFUNC<uint32_t>  MmVerifierData;
        PFUNC<uint32_t>  MmAllocateNonPagedPool;
        PFUNC<uint32_t>  MmPeakCommitment;
        PFUNC<uint32_t>  MmTotalCommitLimitMaximum;
        PFUNC<uint32_t>  CmNtCSDVersion;
        PFUNC<uint32_t>  MmPhysicalMemoryBlock; //PPHYSICAL_MEMORY_DESCRIPTOR*
        PFUNC<uint32_t>  MmSessionBase;
        PFUNC<uint32_t>  MmSessionSize;
        PFUNC<uint32_t>  Unknown7;

};

#define PAE_ENABLED  (1<<5)


enum BsodCodes
{
    CRITICAL_OBJECT_TERMINATION = 0xf4
};

class BlueScreenInterceptor : public Plugin
{
    S2E_PLUGIN
public:
    BlueScreenInterceptor(S2E* s2e): Plugin(s2e) {}

    void initialize();

private:
    WindowsMonitor *m_monitor;

    void onTranslateBlockStart(
        ExecutionSignal *signal,
        S2EExecutionState *state,
        TranslationBlock *tb,
        uint64_t pc);

    void dumpCriticalObjectTermination(S2EExecutionState *state);
    void dispatchErrorCodes(S2EExecutionState *state);

    void onBsod(S2EExecutionState *state, uint64_t pc);

    uint32_t readAndConcretizeRegister(S2EExecutionState *state, unsigned offset);
    bool saveContext(S2EExecutionState *state, s2e::windows::CONTEXT32 &ctx);
    void generateCrashDump(S2EExecutionState *state);
    bool initializeHeader(S2EExecutionState *state, DUMP_HEADER32 *hdr);
};




} // namespace plugins
} // namespace s2e

#endif // S2E_PLUGINS_EXAMPLE_H
