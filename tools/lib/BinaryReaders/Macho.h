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
 * Parts of this file are:
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 *
 *
 * Currently maintained by:
 *    Vitaly Chipounov <vitaly.chipounov@epfl.ch>
 *    Volodymyr Kuznetsov <vova.kuznetsov@epfl.ch>
 *
 * All contributors are listed in S2E-AUTHORS file.
 *
 */
#ifndef S2ETOOLS_MACHO_H
#define S2ETOOLS_MACHO_H

#include <inttypes.h>
#include <vector>
#include "llvm/Support/MemoryBuffer.h"

#include "Binary.h"

namespace s2etools {

namespace macos {

#define MACHO_SIGNATURE                 0xFEEDFACE      // MZ

typedef uint32_t cpu_type_t;
typedef uint32_t cpu_subtype_t;

struct macho_header {
    uint32_t magic;
    cpu_type_t cputype;
    cpu_subtype_t cpusubtype;
    uint32_t filetype;
    uint32_t ncmds;
    uint32_t sizeofcmds;
    uint32_t flags;
}__attribute__((packed));

#define	CPU_ARCH_MASK	0xff000000		/* mask for architecture bits */
#define CPU_ARCH_ABI64	0x01000000		/* 64 bit ABI */


#define CPU_TYPE_X86		((cpu_type_t) 7)
#define CPU_TYPE_I386		CPU_TYPE_X86		/* compatibility */
#define	CPU_TYPE_X86_64		(CPU_TYPE_X86 | CPU_ARCH_ABI64)

#define CPU_SUBTYPE_LITTLE_ENDIAN	((cpu_subtype_t) 0)


#define LC_SEGMENT      0x1     /* segment of this file to be mapped */
#define LC_SYMTAB       0x2     /* link-edit stab symbol table info */
#define LC_SYMSEG       0x3     /* link-edit gdb symbol table info (obsolete) */
#define LC_THREAD       0x4     /* thread */
#define LC_UNIXTHREAD   0x5     /* unix thread (includes a stack) */
#define LC_LOADFVMLIB   0x6     /* load a specified fixed VM shared library */
#define LC_IDFVMLIB     0x7     /* fixed VM shared library identification */
#define LC_IDENT        0x8     /* object identification info (obsolete) */
#define LC_FVMFILE      0x9     /* fixed VM file inclusion (internal use) */
#define LC_PREPAGE      0xa     /* prepage command (internal use) */
#define LC_DYSYMTAB     0xb     /* dynamic link-edit symbol table info */
#define LC_LOAD_DYLIB   0xc     /* load a dynamically linked shared library */
#define LC_ID_DYLIB     0xd     /* dynamically linked shared lib ident */
#define LC_LOAD_DYLINKER 0xe    /* load a dynamic linker */
#define LC_ID_DYLINKER  0xf     /* dynamic linker identification */
#define LC_PREBOUND_DYLIB 0x10  /* modules prebound for a dynamically */
                                /*  linked shared library */
#define LC_ROUTINES     0x11    /* image routines */
#define LC_SUB_FRAMEWORK 0x12   /* sub framework */
#define LC_SUB_UMBRELLA 0x13    /* sub umbrella */
#define LC_SUB_CLIENT   0x14    /* sub client */
#define LC_SUB_LIBRARY  0x15    /* sub library */
#define LC_TWOLEVEL_HINTS 0x16  /* two-level namespace lookup hints */
#define LC_PREBIND_CKSUM  0x17  /* prebind checksum */



struct macho_load_command
{
    uint32_t cmd;
    uint32_t cmdsize;
}__attribute__((packed));

typedef int             vm_prot_t;

struct macho_segment_command
{
    uint32_t cmd;
    uint32_t cmdsize;
    char segname[16];
    uint32_t vmaddr;
    uint32_t vmsize;
    uint32_t fileoff;
    uint32_t filesize;
    vm_prot_t maxprot;
    vm_prot_t initprot;
    uint32_t nsects;
    uint32_t flags;
}__attribute__((packed));

struct macho_section
{
    char sectname[16];
    char segname[16];
    uint32_t addr;
    uint32_t size;
    uint32_t offset;
    uint32_t align;
    uint32_t reloff;
    uint32_t nreloc;
    uint32_t flags;
    uint32_t reserved1;
    uint32_t reserved2;
}__attribute__((packed));


#define	S_NON_LAZY_SYMBOL_POINTERS	0x6	/* section with only non-lazy
                                                   symbol pointers */
#define	S_LAZY_SYMBOL_POINTERS		0x7	/* section with only lazy symbol
                                                   pointers */
#define	S_SYMBOL_STUBS			0x8	/* section with only symbol
                                                   stubs, byte size of stub in
                                                   the reserved2 field */

struct macho_symtab_command {
        uint32_t	cmd;		/* LC_SYMTAB */
        uint32_t	cmdsize;	/* sizeof(struct symtab_command) */
        uint32_t	symoff;		/* symbol table offset */
        uint32_t	nsyms;		/* number of symbol table entries */
        uint32_t	stroff;		/* string table offset */
        uint32_t	strsize;	/* string table size in bytes */
};


struct macho_dysymtab_command {
    uint32_t cmd;	/* LC_DYSYMTAB */
    uint32_t cmdsize;	/* sizeof(struct dysymtab_command) */

    uint32_t ilocalsym;	/* index to local symbols */
    uint32_t nlocalsym;	/* number of local symbols */

    uint32_t iextdefsym;/* index to externally defined symbols */
    uint32_t nextdefsym;/* number of externally defined symbols */

    uint32_t iundefsym;	/* index to undefined symbols */
    uint32_t nundefsym;	/* number of undefined symbols */

    uint32_t tocoff;	/* file offset to table of contents */
    uint32_t ntoc;	/* number of entries in table of contents */

    uint32_t modtaboff;	/* file offset to module table */
    uint32_t nmodtab;	/* number of module table entries */

    uint32_t extrefsymoff;	/* offset to referenced symbol table */
    uint32_t nextrefsyms;	/* number of referenced symbol table entries */

    uint32_t indirectsymoff; /* file offset to the indirect symbol table */
    uint32_t nindirectsyms;  /* number of indirect symbol table entries */

     uint32_t extreloff;	/* offset to external relocation entries */
    uint32_t nextrel;	/* number of external relocation entries */

    uint32_t locreloff;	/* offset to local relocation entries */
    uint32_t nlocrel;	/* number of local relocation entries */

}__attribute__((packed));

struct relocation_info {
   int32_t	r_address;	/* offset in the section to what is being
                                   relocated */
   uint32_t     r_symbolnum:24,	/* symbol index if r_extern == 1 or section
                                   ordinal if r_extern == 0 */
                r_pcrel:1, 	/* was relocated pc relative already */
                r_length:2,	/* 0=byte, 1=word, 2=long, 3=quad */
                r_extern:1,	/* does not include value of sym referenced */
                r_type:4;	/* if not 0, machine specific relocation type */
}__attribute__((packed));
#define	R_ABS	0		/* absolute relocation type for Mach-O files */


}  //macos

class BFDInterface;

/**
 * Parses advanced features of Mach-O object files.
 * Mostly linking and relocating
 */
class MachoReader: public Binary {
private:
    typedef std::vector<macos::macho_section> Sections;
    typedef std::map<std::string, uint64_t> NameToAddress;

    llvm::MemoryBuffer *m_file;

    macos::macho_dysymtab_command m_dynSymCmd;
    Sections m_sections;
    Imports m_imports;
    NameToAddress m_importsByName;

    RelocationEntries m_relocations;

    //Where to allocate the next external import
    uint64_t m_nextAvailableAddress;


    bool resolveImports();
    bool resolveRelocations();
    bool parse();
    bool initialize();

public:
    MachoReader(BFDInterface *bfd);

    //Check that the given buffer contains a valid Mach-O object file
    static bool isValid(llvm::MemoryBuffer *file);

    virtual const Imports &getImports() const {
        return m_imports;
    }

    virtual const RelocationEntries &getRelocations() const {
        return m_relocations;
    }

};


}  //s2etools

#endif
