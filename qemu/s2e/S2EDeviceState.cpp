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

extern "C" {
#include <qemu-common.h>
#include <block.h>

    void vm_stop(int reason);
    void vm_start(void);

    int s2e_dev_snapshot_enable = 0;
}

#include "s2e_block.h"

#include <iostream>
#include <sstream>
#include <s2e/Utils.h>
#include <s2e/S2E.h>
#include "llvm/Support/CommandLine.h"
#include "S2EDeviceState.h"
#include "S2EExecutionState.h"

namespace {
    //Force writes to disk to be persistent (and disable copy on write)
    llvm::cl::opt<bool>
    PersistentDiskWrites("s2e-persistent-disk-writes",
                     llvm::cl::init(false));
    //Share device state between states
    llvm::cl::opt<std::string>
    SharedDevices("s2e-shared-devices",
        llvm::cl::desc("Comma-separated list of devices to be shared between states."),
        llvm::cl::init(""));
}


using namespace s2e;
using namespace std;

unsigned int S2EDeviceState::s_PreferedStateSize = 0x1000;
S2EDeviceState *S2EDeviceState::s_CurrentState = NULL;
std::vector<void *> S2EDeviceState::s_Devices;
bool S2EDeviceState::s_DevicesInited=false;



#define REGISTER_DEVICE(dev) { if (!strcmp(s2e_qemu_get_se_idstr(se), dev)) { s_Devices.push_back(se); }}

S2EDeviceState::S2EDeviceState(const S2EDeviceState &)
{

}

//This is assumed to be called on fork.
//At that time, we need to save the state of the VM to 
//be later restored.
//XXX: use reference counting to delete the device states
void S2EDeviceState::clone(S2EDeviceState **state1, S2EDeviceState **state2)
{
    //We must make two copies

    S2EDeviceState* copy1 = new S2EDeviceState();
    copy1->m_Parent = this;
    copy1->m_State = 0;
    copy1->m_StateSize = 0;
    copy1->m_canTransferSector = m_canTransferSector;
    *state1 = copy1;

    S2EDeviceState* copy2 = new S2EDeviceState();
    copy2->m_Parent = this;
    copy2->m_State = 0;
    copy2->m_StateSize = 0;
    copy2->m_canTransferSector = m_canTransferSector;
    *state2 = copy2;
}

void S2EDeviceState::cloneDiskState()
{
    foreach2(it, m_BlockDevices.begin(), m_BlockDevices.end()) {
        SectorMap &sm = (*it).second;
        foreach2(smit, sm.begin(), sm.end()) {
            uint8_t *newSector = new uint8_t[512];
            memcpy(newSector, (*smit).second, 512);
            (*smit).second = newSector;
        }
    }
}

S2EDeviceState::S2EDeviceState()
{
    m_Parent = NULL;
    m_canTransferSector = true;
}

S2EDeviceState::~S2EDeviceState()
{
    /* TODO */
}

void S2EDeviceState::initDeviceState()
{
    m_State = NULL;
    m_StateSize = 0;
    
    assert(!s_DevicesInited);

    std::set<std::string> ignoreList;
    std::stringstream ss(SharedDevices);
    std::string s;
    while (getline(ss, s, ',')) {
        ignoreList.insert(s);
    }
    //S2E manages the memory and disc state on its own
    ignoreList.insert("ram");
    ignoreList.insert("block");

    //The CPU is taken care of be the S2E executor
    //XXX: What about watchpoints and stuff like that?
    ignoreList.insert("cpu");

    g_s2e->getMessagesStream() << "Initing initial device state." << '\n';

    if (!s_DevicesInited) {
        void *se;
        g_s2e->getDebugStream() << "Looking for relevant virtual devices...";

        //Register all active devices detected by QEMU
        for(se = s2e_qemu_get_first_se();
            se != NULL; se = s2e_qemu_get_next_se(se)) {
                std::string deviceId(s2e_qemu_get_se_idstr(se));
                if (!ignoreList.count(deviceId)) {
                    g_s2e->getDebugStream() << "   Registering device " << deviceId << '\n';
                    s_Devices.push_back(se);
                } else {
                    g_s2e->getDebugStream() << "   Shared device " << deviceId << '\n';
                }
        }
        s_DevicesInited = true;
    }

    if (!PersistentDiskWrites) {
        g_s2e->getMessagesStream() <<
                "WARNING!!! All writes to disk will be lost after shutdown." << '\n';
        __hook_bdrv_read = s2e_bdrv_read;
        __hook_bdrv_write = s2e_bdrv_write;
        __hook_bdrv_aio_read = s2e_bdrv_aio_read;
        __hook_bdrv_aio_write = s2e_bdrv_aio_write;
    }else {
        g_s2e->getMessagesStream() <<
                "WARNING!!! All disk writes will be SHARED across states! BEWARE OF CORRUPTION!" << '\n';
    }
    //saveDeviceState();
    //restoreDeviceState();
}

int s2edev_dbg=0;

void S2EDeviceState::saveDeviceState()
{
    s2e_dev_snapshot_enable = 1;
    vm_stop(0);
    m_Offset = 0;
    assert(s_CurrentState == NULL);
    s_CurrentState = this;

    //DPRINTF("Saving device state %p\n", this);
    /* Iterate through all device descritors and call
    * their snapshot function */
    for (vector<void*>::iterator it = s_Devices.begin(); it != s_Devices.end(); it++) {
        //unsigned o = m_Offset;
        void *se = *it;
        //DPRINTF("%s ", s2e_qemu_get_se_idstr(se));
        s2e_qemu_save_state(se);
        //DPRINTF("sz=%d - ", m_Offset - o);
    }
    //DPRINTF("\n");

    ShrinkBuffer();
    s2e_dev_snapshot_enable = 0;
    s_CurrentState = NULL;
    vm_start();
}

void S2EDeviceState::restoreDeviceState()
{
    assert(s_CurrentState == NULL);
    assert(m_StateSize);
    assert(m_State);

    s_CurrentState = this;

    vm_stop(0);
    s2e_dev_snapshot_enable = 1;

    m_Offset = 0;

    //DPRINTF("Restoring device state %p\n", this);
    for (vector<void*>::iterator it = s_Devices.begin(); it != s_Devices.end(); it++) {
        //unsigned o = m_Offset;
        void *se = *it;
        //DPRINTF("%s ", s2e_qemu_get_se_idstr(se));  
        s2e_qemu_load_state(se);
        //DPRINTF("sz=%d - ", m_Offset - o);
    }
    //DPRINTF("\n");

    s2e_dev_snapshot_enable = 0;
    s_CurrentState = NULL;
    vm_start();
}


bool S2EDeviceState::canTransferSector() const
{
    return m_canTransferSector;
}

/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/

void S2EDeviceState::AllocateBuffer(unsigned int Sz)
{
    if (!m_State) {
        unsigned int NewSize = Sz < 256 ? 256 : Sz;
        m_State = (unsigned char *)malloc(NewSize);
        if (!m_State) {
            cerr << "Cannot allocate memory for device state snapshot" << endl;
            exit(-1);
        }
        m_StateSize = NewSize;
        m_Offset = 0;
        return;
    }

    if (m_Offset + Sz >= m_StateSize) {
        /* Need to expand the buffer */
        unsigned int NewSize = Sz < 256 ? 256 : Sz;
        m_StateSize += NewSize;
        m_State = (unsigned char*)realloc(m_State, m_StateSize);
        if (!m_State) {
            cerr << "Cannot reallocate memory for device state snapshot" << endl;
            exit(-1);
        }
        return;
    }
}

void S2EDeviceState::ShrinkBuffer()
{
    if (m_Offset != m_StateSize) {
        unsigned char *NewBuf = (unsigned char*)realloc(m_State, m_Offset);
        if (!NewBuf) {
            cerr << "Cannot shrink device state snapshot" << endl;
            return;
        }
        m_State = NewBuf;
        m_StateSize = m_Offset;
    }
}

void S2EDeviceState::PutByte(int v)
{
    AllocateBuffer(1);
    m_State[m_Offset++] = v;
}

void S2EDeviceState::PutBuffer(const uint8_t *buf, int size1)
{
    AllocateBuffer(size1);
    memcpy(&m_State[m_Offset], buf, size1);
    m_Offset += size1;
}

int S2EDeviceState::GetByte()
{
    assert(m_Offset + 1 <= m_StateSize);
    return m_State[m_Offset++];
}

int S2EDeviceState::GetBuffer(uint8_t *buf, int size1)
{
    assert(m_Offset + size1 <= m_StateSize);
    memcpy(buf, &m_State[m_Offset], size1);
    m_Offset += size1;
    return size1;
}

/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/

int S2EDeviceState::writeSector(struct BlockDriverState *bs, int64_t sector, const uint8_t *buf, int nb_sectors)
{
    SectorMap &dev = m_BlockDevices[bs];
 //   DPRINTF("writeSector %#"PRIx64" count=%d\n", sector, nb_sectors);
    for (int64_t i = sector; i<sector+nb_sectors; i++) {
        SectorMap::iterator it = dev.find(i);
        uint8_t *secbuf;
        
        if (it == dev.end()) {
            secbuf = new uint8_t[512];
            dev[i] = secbuf;            
        }else {
            secbuf = (*it).second;
        }

        memcpy(secbuf, buf, 512);
        buf+=512;
    }
    return 0;
}


int S2EDeviceState::readSector(struct BlockDriverState *bs, int64_t sector, uint8_t *buf, int nb_sectors,
                               s2e_raw_read fb)
{
    bool hasRead = false;
  //  DPRINTF("readSector %#"PRIx64" count=%d\n", sector, nb_sectors);
    for (int64_t i = sector; i<sector+nb_sectors; i++) {
        for (S2EDeviceState *curState = this; curState; curState = curState->m_Parent) {
            SectorMap &dev = curState->m_BlockDevices[bs];
            SectorMap::iterator it = dev.find(i);
            
            if (it != dev.end()) {
                memcpy(buf, (*it).second, 512);
                buf+=512;
                hasRead = true;
                break;
            }
        }
        //Did not find any written sector, read from the original disk
        if (!hasRead) {
            m_canTransferSector = false;
            int ret = fb(bs, i, buf, 1);
            m_canTransferSector = true;
            if(ret < 0) {
                return ret;
            }
            buf+=512;
            hasRead = false;
        }
    }
    return 0;
}

/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/

/**
*  Functions facing QEMU. They simmply forward the call to the right
*  device state.
*/

extern "C" {

void s2e_qemu_put_byte(S2EExecutionState *s, int v)
{
    S2EDeviceState::getCurrentVmState()->PutByte(v);
}

int s2e_qemu_get_byte(S2EExecutionState *s)
{
    return S2EDeviceState::getCurrentVmState()->GetByte();
}

int s2e_qemu_get_buffer(S2EExecutionState *s, uint8_t *buf, int size1)
{
    return S2EDeviceState::getCurrentVmState()->GetBuffer(buf, size1);
}

void s2e_qemu_put_buffer(S2EExecutionState *s, const uint8_t *buf, int size)
{
    S2EDeviceState::getCurrentVmState()->PutBuffer(buf, size);
}

void s2e_init_device_state(S2EExecutionState *s)
{
    s->getDeviceState()->initDeviceState();
}


int s2e_bdrv_read(BlockDriverState *bs, int64_t sector_num,
                  uint8_t *buf, int nb_sectors, int *fallback,
                  s2e_raw_read fb)
{
    S2EDeviceState *devState = g_s2e_state->getDeviceState();
    if (devState->canTransferSector()) {
        *fallback = 0;
        return devState->readSector(bs, sector_num, buf, nb_sectors, fb);
    }else {
        *fallback = 1;
        return 0;
    }
}

int s2e_bdrv_write(BlockDriverState *bs, int64_t sector_num,
                   const uint8_t *buf, int nb_sectors
                   )
{
    S2EDeviceState *devState = g_s2e_state->getDeviceState();
    
    return devState->writeSector(bs, sector_num, buf, nb_sectors);

}

BlockDriverAIOCB *s2e_bdrv_aio_read(
                                    BlockDriverState *bs, int64_t sector_num,
                                    uint8_t *buf, int nb_sectors,
                                    BlockDriverCompletionFunc *cb, void *opaque,
                                    int *fallback, s2e_raw_read fb)
{
    S2EDeviceState *devState = g_s2e_state->getDeviceState();
    if (devState->canTransferSector()) {
        *fallback = 0;
        int ret = devState->readSector(bs, sector_num, buf, nb_sectors, fb);
        cb(opaque, ret);
        return NULL;
    }else {
        *fallback = 1;
        return NULL;
    }
}

BlockDriverAIOCB *s2e_bdrv_aio_write(
                                     BlockDriverState *bs, int64_t sector_num,
                                     const uint8_t *buf, int nb_sectors,
                                     BlockDriverCompletionFunc *cb, void *opaque)
{
    int ret;
    S2EDeviceState *devState = g_s2e_state->getDeviceState();
    ret = devState->writeSector(bs, sector_num, (uint8_t*)buf, nb_sectors);
    cb(opaque, ret);
    return NULL;
}


void s2e_bdrv_aio_cancel(BlockDriverAIOCB *acb)
{

}

}
