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

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/mman.h>
#endif

#include "Log.h"
#include "MemoryFile.h"

namespace s2etools
{

MemoryFile::MemoryFile(const std::string &fileName)
{
    TAG="MemoryFile";

    m_size = 0;
    m_file = NULL;

#ifdef _WIN32
    m_hFile = CreateFile(fileName.c_str(), GENERIC_READ,
                              FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                              NULL);
    if (m_hFile == INVALID_HANDLE_VALUE) {
        return;
    }

    LARGE_INTEGER FileSize;
    if (!GetFileSizeEx(m_hFile, &FileSize)) {
        CloseHandle(m_hFile);
        return;
    }

    m_hMapping = CreateFileMapping(m_hFile, NULL, PAGE_READONLY, FileSize.HighPart, FileSize.LowPart, NULL);
    if (m_hMapping == NULL) {
        CloseHandle(m_hFile);
        return;
    }

    m_file = MapViewOfFile(m_hMapping, FILE_MAP_COPY, FileSize.HighPart, FileSize.LowPart, 0);
    if (!m_file) {
        CloseHandle(m_hMapping);
        CloseHandle(m_hFile);
        return;
    }

    m_size = FileSize.QuadPart;

#else
    int file = open(fileName.c_str(), O_RDONLY);
    if (file<0) {
        LOGERROR() << "LogParser: Could not open " << fileName << std::endl;
        return;
    }

    off_t fileSize = lseek(file, 0, SEEK_END);
    if (fileSize == (off_t) -1) {
        LOGERROR() << "Could not get log file size" << std::endl;
        return;
    }

    m_file = mmap(NULL, fileSize, PROT_READ|PROT_WRITE, MAP_PRIVATE, file, 0);
    if (!m_file) {
        LOGERROR() << "Could not map the log file in memory" << std::endl;
        close(file);
        return;
    }

    m_size = fileSize;

#endif

}

MemoryFile::~MemoryFile()
{
#ifdef _WIN32
    UnmapViewOfFile(m_file);
    CloseHandle(m_hMapping);
    CloseHandle(m_hFile);
#else
    if (m_file) {
        munmap(m_file, m_size);
    }
#endif

}

uint8_t *MemoryFile::getBuffer()
{
    return (uint8_t*)m_file;
}

}
