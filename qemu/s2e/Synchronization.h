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

#ifndef S2E_SYNCHRONIZATION_H
#define S2E_SYNCHRONIZATION_H

#include <inttypes.h>
#include <string>

namespace s2e {

class S2ESynchronizedObjectInternal {
private:
    uint8_t *m_sharedBuffer;
    unsigned m_size;
    unsigned m_headerSize;

    S2ESynchronizedObjectInternal() {

    }

public:
    S2ESynchronizedObjectInternal(unsigned size);
    ~S2ESynchronizedObjectInternal();

    void lock();
    void release();
    void *aquire();
    void *tryAquire();

    //Unsynchronized function to get the buffer
    void *get() const {
        return ((uint8_t*)m_sharedBuffer)+m_headerSize;
    }
};

/**
 *  This class creates a shared memory buffer on which
 *  all S2E processes can perform read/write requests.
 */
template <class T>
class S2ESynchronizedObject {
private:
    S2ESynchronizedObjectInternal sync;


public:

    S2ESynchronizedObject():sync(S2ESynchronizedObjectInternal(sizeof(T))) {
        new (sync.get()) T();
    }

    ~S2ESynchronizedObject() {
        T* t = (T*)sync.get();
        t->~T();
    }

    T *acquire() {
        return (T*)sync.aquire();
    }

    //Returns null if could not lock the object
    T *tryAcquire() {
        return (T*)sync.tryAquire();
    }

    void release() {
        sync.release();
    }

    T* get() const {
        return (T*)sync.get();
    }

};

class AtomicFunctions {
public:
    static uint64_t read(uint64_t *address);
    static void write(uint64_t *address, uint64_t value);
    static void add(uint64_t *address, uint64_t value);
    static void sub(uint64_t *address, uint64_t value);
};

template <class T>
class AtomicObject {
private:
    mutable uint64_t m_value;

public:
    AtomicObject() {}

    T read() const{
        uint64_t value = AtomicFunctions::read(&m_value);
        return *(T*)&value;
    }

    void write(T &object) {
        AtomicFunctions::write(&m_value, *(uint64_t*)&object);
    }
};

}

#endif
