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

#include <cassert>

#include "S2E.h"
#include "config-host.h"
#include "Synchronization.h"

#ifndef CONFIG_WIN32
#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <iostream>
#include <stdlib.h>
#endif

#ifdef CONFIG_DARWIN
#include <mach/semaphore.h>
#else
#ifndef _WIN32
#include <semaphore.h>
#else

#endif
#include <errno.h>
#endif


namespace s2e {

#if defined(CONFIG_WIN32)
#warning Synchronized objects not implemented on Windows!

S2ESynchronizedObjectInternal::S2ESynchronizedObjectInternal(unsigned size) {
    m_size = size;
    m_headerSize = 0;

    m_sharedBuffer = new uint8_t[size];
}

S2ESynchronizedObjectInternal::~S2ESynchronizedObjectInternal()
{
    delete [] m_sharedBuffer;
}

void *S2ESynchronizedObjectInternal::aquire()
{
    return m_sharedBuffer;
}

void *S2ESynchronizedObjectInternal::tryAquire()
{
    return m_sharedBuffer;
}

void S2ESynchronizedObjectInternal::release()
{

}

uint64_t AtomicFunctions::read(uint64_t *address)
{
   return __sync_fetch_and_add(address, 0);
}

void AtomicFunctions::add(uint64_t *address, uint64_t value)
{
    __sync_fetch_and_add(address, value);
}

void AtomicFunctions::sub(uint64_t *address, uint64_t value)
{
    __sync_fetch_and_sub(address, value);
}


void AtomicFunctions::write(uint64_t *address, uint64_t value)
{
    *address = value;
}

#else



struct SyncHeader{
#ifdef CONFIG_DARWIN
    unsigned lock;
#else
    sem_t lock;
#endif
};


S2ESynchronizedObjectInternal::S2ESynchronizedObjectInternal(unsigned size) {
    m_size = size;
    m_headerSize = sizeof(SyncHeader);

    unsigned totalSize = m_headerSize + size;

    m_sharedBuffer = (uint8_t*)mmap(NULL, totalSize, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANON, -1, 0);
    if (!m_sharedBuffer) {
        perror("Could not allocate shared memory ");
        exit(-1);
    }

    SyncHeader *hdr = static_cast<SyncHeader*>((void*)m_sharedBuffer);

#ifdef CONFIG_DARWIN
    hdr->lock = 1;
#else
    if (sem_init(&hdr->lock, 1, 1) < 0) {
        perror("Could not initialize semaphore for shared memory region");
        exit(-1);
    }
#endif
}


S2ESynchronizedObjectInternal::~S2ESynchronizedObjectInternal()
{
    //SyncHeader *hdr = (SyncHeader*)m_sharedBuffer;

    //XXX: What about closing the semaphore, IPC?

    unsigned totalSize = m_headerSize + m_size;
    munmap(m_sharedBuffer, totalSize);
}

void *S2ESynchronizedObjectInternal::aquire() {
    SyncHeader *hdr = (SyncHeader*)m_sharedBuffer;
#ifdef CONFIG_DARWIN
    while (__sync_lock_test_and_set(&hdr->lock, 0) != 0);
#else
    int ret;

     do {
        ret = sem_wait(&hdr->lock);
        if (ret < 0) {
            assert(errno != EDEADLK && errno != ENOSYS && errno != EINVAL);
        }
     }while(ret);

#endif
    return ((uint8_t*)m_sharedBuffer + m_headerSize);
}

void *S2ESynchronizedObjectInternal::tryAquire()
{
    SyncHeader *hdr = (SyncHeader*)m_sharedBuffer;
#ifdef CONFIG_DARWIN
    if (__sync_lock_test_and_set(&hdr->lock, 0) != 0) {
        return NULL;
    }
#else
    int ret;

     do {
        ret = sem_trywait(&hdr->lock);
        if (ret < 0 && errno == EAGAIN) {
            return NULL;
        }
     }while(ret);

#endif
    return ((uint8_t*)m_sharedBuffer + m_headerSize);
}


void S2ESynchronizedObjectInternal::release()
{
    SyncHeader *hdr = (SyncHeader*)m_sharedBuffer;
#ifdef CONFIG_DARWIN
    hdr->lock = 1;
#else
    if (sem_post(&hdr->lock) < 0) {
        assert(false && "Semaphore failed");
    }
#endif
}

uint64_t AtomicFunctions::read(uint64_t *address)
{
    return __sync_fetch_and_add(address, 0);
}

void AtomicFunctions::add(uint64_t *address, uint64_t value)
{
    __sync_fetch_and_add(address, value);
}

void AtomicFunctions::sub(uint64_t *address, uint64_t value)
{
    __sync_fetch_and_sub(address, value);
}

void AtomicFunctions::write(uint64_t *address, uint64_t value)
{
    *address = value;
}

#endif

}
