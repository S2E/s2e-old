/* Copyright (C) 2007-2010 The Android Open Source Project
**
** This software is licensed under the terms of the GNU General Public
** License version 2, as published by the Free Software Foundation, and
** may be copied, distributed, and modified under those terms.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
*/

/*
 * Contains implementation of a class ElfMappedSection, that encapsulates
 * a section of an ELF file, mapped to memory.
 */

#include "elf_defs.h"
#include "elf_mapped_section.h"

ElfMappedSection::ElfMappedSection()
    : mapped_at_(NULL),
      data_(NULL),
      size_(0) {
}

ElfMappedSection::~ElfMappedSection() {
  if (mapped_at_ != NULL) {
#ifdef WIN32
    UnmapViewOfFile(mapped_at_);
#else   // WIN32
    munmap(mapped_at_, diff_ptr(mapped_at_, data_) + size_);
#endif  // WIN32
  }
}

bool ElfMappedSection::map(ELF_FILE_HANDLE handle,
                           Elf_Xword offset,
                           Elf_Word size) {
  /* Get the mask for mapping offset alignment. */
#ifdef  WIN32
  SYSTEM_INFO sys_info;
  GetSystemInfo(&sys_info);
  const Elf_Xword align_mask = sys_info.dwAllocationGranularity - 1;
#else   // WIN32
  const Elf_Xword align_mask = getpagesize() - 1;
#endif  // WIN32

  /* Adjust mapping offset and mapping size accordingly to
   * the mapping alignment requirements. */
  const Elf_Xword map_offset = offset & ~align_mask;
  const Elf_Word map_size = static_cast<Elf_Word>(offset - map_offset + size);

  /* Make sure mapping size doesn't exceed 4G: may happen on 64-bit ELFs, if
   * section size is close to 4G, while section offset is badly misaligned. */
  assert(map_size >= size);
  if (map_size < size) {
    _set_errno(EFBIG);
    return false;
  }

  /* Map the section. */
#ifdef  WIN32
  LARGE_INTEGER converter;
  converter.QuadPart = map_offset + map_size;
  HANDLE map_handle = CreateFileMapping(handle, NULL, PAGE_READONLY,
                                        converter.HighPart, converter.LowPart,
                                        NULL);
  assert(map_handle != NULL);
  if (map_handle != NULL) {
    converter.QuadPart = map_offset;
    mapped_at_ = MapViewOfFile(map_handle, FILE_MAP_READ, converter.HighPart,
                               converter.LowPart, map_size);
    assert(mapped_at_ != NULL);
    /* Memory mapping (if successful) will hold extra references to the
     * mapping, so we can close it right after we mapped file view. */
    CloseHandle(map_handle);
  }
  if (mapped_at_ == NULL) {
    _set_errno(GetLastError());
    return false;
  }
#else   // WIN32
  mapped_at_ = mmap(0, map_size, PROT_READ, MAP_SHARED, handle, map_offset);
  assert(mapped_at_ != MAP_FAILED);
  if (mapped_at_ == MAP_FAILED) {
    return false;
  }
#endif  // WIN32

  data_ = INC_CPTR(mapped_at_, offset - map_offset);
  size_ = size;

  return true;
}
