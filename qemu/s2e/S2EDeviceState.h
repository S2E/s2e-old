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
 *    Volodymyr Kuznetsov <vova.kuznetsov@epfl.ch>
 *    Vitaly Chipounov <vitaly.chipounov@epfl.ch>
 *
 * All contributors are listed in S2E-AUTHORS file.
 *
 */

#ifndef _S2E_DEVICE_STATE_H_

#define _S2E_DEVICE_STATE_H_

extern "C" {
#include "hw/hw.h"
}

#include <vector>
#include <map>
#include <set>
#include <stdint.h>

#include "s2e_block.h"

namespace s2e {

class S2EExecutionState;

class S2EDeviceState {
private:
    typedef std::map<int64_t, uint8_t *> SectorMap;
    typedef std::map<BlockDriverState *, SectorMap> BlockDeviceToSectorMap;

    static std::vector<void *> s_devices;
    static std::set<std::string> s_customDevices;
    static bool s_devicesInited;

    QEMUFile *m_memFile;
    unsigned char *m_state;
    unsigned int m_stateSize;

    static unsigned int s_preferedStateSize;

    S2EDeviceState *m_parent;
    BlockDeviceToSectorMap m_blockDevices;
    
    void allocateBuffer(unsigned int Sz);

    void cloneDiskState();

    S2EDeviceState(const S2EDeviceState &);
public:
    static S2EDeviceState *s_currentDeviceState;

    S2EDeviceState();
    ~S2EDeviceState();

    void clone(S2EDeviceState **state1, S2EDeviceState **state2);
    void initDeviceState();

    //From QEMU to KLEE
    void saveDeviceState();
    
    //From KLEE to QEMU
    void restoreDeviceState();

    int putBuffer(const uint8_t *buf, int64_t pos, int size);
    int getBuffer(uint8_t *buf, int64_t pos, int size);

    int writeSector(struct BlockDriverState *bs, int64_t sector, const uint8_t *buf, int nb_sectors);
    int readSector(struct BlockDriverState *bs, int64_t sector, uint8_t *buf, int nb_sectors);
};

}

#endif
