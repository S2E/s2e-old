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


#include "WindowsMonitor.h"
#include "WindowsImage.h"

#include <s2e/Utils.h>
#include <s2e/s2e_qemu.h>

#include <algorithm>

using namespace std;

namespace s2e {
namespace windows {

const char * s_irpMjArray [] = {
    "IRP_MJ_CREATE",
    "IRP_MJ_CREATE_NAMED_PIPE",
    "IRP_MJ_CLOSE",
    "IRP_MJ_READ",
    "IRP_MJ_WRITE",
    "IRP_MJ_QUERY_INFORMATION",
    "IRP_MJ_SET_INFORMATION",
    "IRP_MJ_QUERY_EA",
    "IRP_MJ_SET_EA",
    "IRP_MJ_FLUSH_BUFFERS",
    "IRP_MJ_QUERY_VOLUME_INFORMATION",
    "IRP_MJ_SET_VOLUME_INFORMATION",
    "IRP_MJ_DIRECTORY_CONTROL",
    "IRP_MJ_FILE_SYSTEM_CONTROL",
    "IRP_MJ_DEVICE_CONTROL",
    "IRP_MJ_INTERNAL_DEVICE_CONTROL",
    "IRP_MJ_SCSI",
    "IRP_MJ_SHUTDOWN",
    "IRP_MJ_LOCK_CONTROL",
    "IRP_MJ_CLEANUP",
    "IRP_MJ_CREATE_MAILSLOT",
    "IRP_MJ_QUERY_SECURITY",
    "IRP_MJ_SET_SECURITY",
    "IRP_MJ_POWER",
    "IRP_MJ_SYSTEM_CONTROL",
    "IRP_MJ_DEVICE_CHANGE",
    "IRP_MJ_QUERY_QUOTA",
    "IRP_MJ_SET_QUOTA",
    "IRP_MJ_PNP",
    "IRP_MJ_PNP_POWER",
    "IRP_MJ_MAXIMUM_FUNCTION"
};

}
}

namespace s2e {
namespace plugins {

bool WindowsImage::IsValidString(const char *str)
{
    for (unsigned i=0; str[i]; i++) {
        if (str[i] > 0x20 && (unsigned)str[i] < 0x80) {
            continue;
        }
        return false;
    }
    return true;
}

WindowsImage::WindowsImage(S2EExecutionState *state, uint64_t Base)
{
    m_Base = Base;
    m_ImageSize = 0;
    m_ImageBase = 0;

    if (!state->readMemoryConcrete(m_Base, &DosHeader, sizeof(DosHeader))) {
        s2e_debug_print("Could not load IMAGE_DOS_HEADER structure (m_Base=%#"PRIx64")\n", m_Base);
        return;
    }

    if (DosHeader.e_magic != s2e::windows::IMAGE_DOS_SIGNATURE)  {
        s2e_debug_print("PE image has invalid magic\n");
        return;
    }

    if (!state->readMemoryConcrete(m_Base+DosHeader.e_lfanew, &NtHeader, sizeof(NtHeader))) {
        s2e_debug_print("Could not load IMAGE_NT_HEADER structure (m_Base=%#"PRIx64")\n", m_Base+(unsigned)DosHeader.e_lfanew);
        return;
    }

    if (NtHeader.Signature != s2e::windows::IMAGE_NT_SIGNATURE) {
        s2e_debug_print("NT header has invalid magic\n");
        return;
    }

    m_ImageSize = NtHeader.OptionalHeader.SizeOfImage;
    m_ImageBase = NtHeader.OptionalHeader.ImageBase;
    assert(m_ImageBase < 0x100000000);
    m_EntryPoint = NtHeader.OptionalHeader.AddressOfEntryPoint;

    m_ImportsInited = false;
    m_ExportsInited = false;
    m_sectionsInited = false;
}

bool WindowsImage::InitSections(S2EExecutionState *state)
{
    unsigned sections = NtHeader.FileHeader.NumberOfSections;

    s2e::windows::IMAGE_SECTION_HEADER sectionHeader;
    uint64_t pSection = m_Base + DosHeader.e_lfanew +
            sizeof(s2e::windows::IMAGE_NT_SIGNATURE) +
            sizeof(s2e::windows::IMAGE_FILE_HEADER) + sizeof(s2e::windows::IMAGE_OPTIONAL_HEADER);

    for (unsigned i=0; i<sections; ++i) {
        if (!state->readMemoryConcrete(pSection, &sectionHeader, sizeof(sectionHeader))) {
            return false;
        }
        SectionDescriptor sectionDesc;
        sectionDesc.loadBase = m_Base + sectionHeader.VirtualAddress;
        sectionDesc.size = sectionHeader.SizeOfRawData;

        for (unsigned i=0; sectionHeader.Name[i] &&  i<IMAGE_SIZEOF_SHORT_NAME; ++i) {
            sectionDesc.name += sectionHeader.Name[i];
        }

        sectionDesc.setWrite(sectionHeader.Characteristics & s2e::windows::IMAGE_SCN_MEM_WRITE);
        sectionDesc.setRead(sectionHeader.Characteristics & s2e::windows::IMAGE_SCN_MEM_READ);
        sectionDesc.setExecute(sectionHeader.Characteristics & s2e::windows::IMAGE_SCN_MEM_EXECUTE);
        m_Sections.push_back(sectionDesc);

        pSection += sizeof(sectionHeader);
    }

    return true;
}

int WindowsImage::InitExports(S2EExecutionState *state)
{

    s2e::windows::PIMAGE_DATA_DIRECTORY ExportDataDir;
    uint32_t ExportTableAddress;
    uint32_t ExportTableSize;

    s2e::windows::PIMAGE_EXPORT_DIRECTORY ExportDir;

    unsigned i;
    int res = 0;
    unsigned TblSz;
    uint32_t *Names;
    uint32_t *FcnPtrs;

    ExportDataDir =  &NtHeader.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    ExportTableAddress = ExportDataDir->VirtualAddress + m_Base;
    ExportTableSize = ExportDataDir->Size;
    s2e_debug_print("ExportTableAddress=%#x ExportTableSize=%#x\n", ExportDataDir->VirtualAddress, ExportDataDir->Size);

    if (!ExportDataDir->VirtualAddress || !ExportTableSize) {
        return -1;
    }

    ExportDir = (s2e::windows::PIMAGE_EXPORT_DIRECTORY)malloc(ExportTableSize);

    if (!state->readMemoryConcrete(ExportTableAddress, (uint8_t*)ExportDir, ExportTableSize)) {
        s2e_debug_print("Could not load PIMAGE_EXPORT_DIRECTORY structures (m_Base=%#x)\n", ExportTableAddress);
        res = -5;
        goto err2;
    }

    TblSz = ExportDir->NumberOfFunctions * sizeof(uint32_t);
    Names = (uint32_t*)malloc(TblSz);
    if (!Names) {
        return -1;
    }

    FcnPtrs = (uint32_t*)malloc(TblSz);
    if (!FcnPtrs) {
        free(Names);
        return -1;
    }

    if (!state->readMemoryConcrete(m_Base + ExportDir->AddressOfNames, Names, TblSz)) {
        s2e_debug_print("Could not load names of exported functions");
        res = -6;
        goto err2;
    }

    if (!state->readMemoryConcrete(m_Base + ExportDir->AddressOfFunctions, FcnPtrs, TblSz)) {
        s2e_debug_print("Could not load addresses of  exported functions");
        res = -7;
        goto err3;
    }

    for (i=0; i<ExportDir->NumberOfFunctions; i++) {
        string FunctionName;

        uint32_t EffAddr = Names[i] + m_Base;
        if (EffAddr < m_Base || EffAddr >= m_Base + m_ImageSize) {
            continue;
        }

        state->readString(Names[i] + m_Base, FunctionName);
        if (!IsValidString(FunctionName.c_str())) {
            continue;
        }

        //s2e_debug_print("Export %s @%#"PRIx64"\n", FunctionName.c_str(), FcnPtrs[i]+m_Base);
        m_Exports[FunctionName] = FcnPtrs[i] + m_Base;
    }

    free(FcnPtrs);
err3: free(Names);
err2: free(ExportDir);
    //err1:
    return res;
}

int WindowsImage::InitImports(S2EExecutionState *state)
{
    s2e::windows::PIMAGE_DATA_DIRECTORY ImportDir;
    uint64_t ImportTableAddress;
    uint32_t ImportTableSize;

    uint64_t ImportNameTable;
    uint64_t ImportAddressTable;

    s2e::windows::PIMAGE_IMPORT_DESCRIPTOR ImportDescriptors;
    unsigned ImportDescCount;
    unsigned i, j;

    ImportDir =  &NtHeader.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    ImportTableAddress = ImportDir->VirtualAddress + m_Base;
    ImportTableSize = ImportDir->Size;

    if (!ImportTableAddress || !ImportTableSize) {
        return -1;
    }

    ImportDescCount = ImportTableSize / sizeof(s2e::windows::IMAGE_IMPORT_DESCRIPTOR);
    ImportDescriptors = (s2e::windows::PIMAGE_IMPORT_DESCRIPTOR)malloc(ImportTableSize);
    if (!ImportDescriptors) {
        s2e_debug_print("Could not allocate memory for import descriptors\n");
        return -5;
    }

    if (!state->readMemoryConcrete(ImportTableAddress, ImportDescriptors, ImportTableSize)) {
        s2e_debug_print("Could not load IMAGE_IMPORT_DESCRIPTOR structures (base=%#"PRIx64")\n", ImportTableAddress);
        free(ImportDescriptors);
        return -6;
    }

    for (i=0; ImportDescriptors[i].Characteristics && i<ImportDescCount; i++) {
        s2e::windows::IMAGE_THUNK_DATA32 INaT;
        string DllName;

        if (!state->readString(ImportDescriptors[i].Name + m_Base, DllName)) {
            continue;
        }
        if (!IsValidString(DllName.c_str())) {
            continue;
        }

        s2e_debug_print("%s\n", DllName.c_str());

        ImportAddressTable = m_Base + ImportDescriptors[i].FirstThunk;
        ImportNameTable = m_Base + ImportDescriptors[i].OriginalFirstThunk;

        j=0;
        do {
            s2e::windows::IMAGE_THUNK_DATA32 IAT;
            uint32_t Name;
            bool res1, res2;
            res1 = state->readMemoryConcrete(ImportAddressTable+j*sizeof(s2e::windows::IMAGE_THUNK_DATA32),
                &IAT, sizeof(IAT));
            res2 = state->readMemoryConcrete(ImportNameTable+j*sizeof(s2e::windows::IMAGE_THUNK_DATA32),
                &INaT, sizeof(INaT));

            if (!res1 || !res2) {
                s2e_debug_print("Could not load IAT entries\n");
                free(ImportDescriptors);
                return -7;
            }

            if (!INaT.u1.AddressOfData)
                break;

            if (INaT.u1.AddressOfData & IMAGE_ORDINAL_FLAG) {
                uint32_t Tmp = INaT.u1.AddressOfData & ~0xFFFF;
                Tmp &= ~IMAGE_ORDINAL_FLAG;
                if (!Tmp) {
                    s2e_debug_print("Does not support import by ordinals\n");
                    break;
                }
            }else {
                INaT.u1.AddressOfData += m_Base;
            }

            string FunctionName;
            Name = INaT.u1.AddressOfData;

            if (Name < m_Base || Name >= m_Base + m_ImageSize) {
                j++;
                continue;
            }

            if (!state->readString(Name+2, FunctionName)) {
                j++;
                continue;
            }

            if (!IsValidString(FunctionName.c_str())) {
                j++;
                continue;
            }
            //s2e_debug_print("  %s @%#x\n", FunctionName.c_str(), IAT.u1.Function);

            std::transform(DllName.begin(), DllName.end(), DllName.begin(),
                           ::tolower);
            ImportedFunctions &ImpFcnIt = m_Imports[DllName];
            ImpFcnIt[FunctionName] = IAT.u1.Function;

            /* Check if we already hooked the given address */
            //iohook_winstrucs_hook_import(FunctionName, IAT.u1.Function);
            j++;
        }while(INaT.u1.AddressOfData);
    }

    free(ImportDescriptors);
    return 0;

}


void WindowsImage::DumpInfo(std::ostream &os) const
{

}

uint64_t WindowsImage::GetRoundedImageSize() const {
  if (m_ImageSize & 0xFFF)
    return (m_ImageSize & ~0xFFF) + 0x1000;
  else
    return m_ImageSize;
}


const Exports& WindowsImage::GetExports(S2EExecutionState *s)
{
    if (!m_ExportsInited) {
        InitExports(s);
        m_ExportsInited = true;
    }
    return m_Exports;
}

const Imports& WindowsImage::GetImports(S2EExecutionState *s)
{
    if (!m_ImportsInited) {
      InitImports(s);
      m_ImportsInited = true;
    }
    return m_Imports;
}

const ModuleSections &WindowsImage::GetSections(S2EExecutionState *s)
{
    if (!m_sectionsInited) {
        InitSections(s);
        m_sectionsInited = true;
    }

    return m_Sections;
}

}
}
