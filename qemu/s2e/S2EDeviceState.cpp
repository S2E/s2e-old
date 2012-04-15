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
#include <qapi-types.h>

void vm_stop(int reason);
void vm_start(void);
}

#include "s2e_block.h"

#include <iostream>
#include <sstream>
#include <s2e/Utils.h>
#include <s2e/S2E.h>
#include <s2e/s2e_qemu.h>
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

unsigned int S2EDeviceState::s_preferedStateSize = 0x1000;
std::vector<void *> S2EDeviceState::s_devices;
bool S2EDeviceState::s_devicesInited=false;
S2EDeviceState* S2EDeviceState::s_currentDeviceState = NULL;

extern "C" {

static int s2e_qemu_get_buffer(uint8_t *buf, int64_t pos, int size)
{
    return S2EDeviceState::s_currentDeviceState->getBuffer(buf, pos, size);
}

static int s2e_qemu_put_buffer(const uint8_t *buf, int64_t pos, int size)
{
    return S2EDeviceState::s_currentDeviceState->putBuffer(buf, pos, size);
}

void s2e_init_device_state(S2EExecutionState *s)
{
    s->getDeviceState()->initDeviceState();
}

}




#define REGISTER_DEVICE(dev) { if (!strcmp(s2e_qemu_get_se_idstr(se), dev)) { s_devices.push_back(se); }}

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
    copy1->m_parent = this;
    copy1->m_state = 0;
    copy1->m_stateSize = 0;
    copy1->m_memFile = m_memFile;
    *state1 = copy1;

    S2EDeviceState* copy2 = new S2EDeviceState();
    copy2->m_parent = this;
    copy2->m_state = 0;
    copy2->m_stateSize = 0;
    copy2->m_memFile = m_memFile;

    *state2 = copy2;
}

void S2EDeviceState::cloneDiskState()
{
    foreach2(it, m_blockDevices.begin(), m_blockDevices.end()) {
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
    m_parent = NULL;
}

S2EDeviceState::~S2EDeviceState()
{
    /* TODO */
}

void S2EDeviceState::initDeviceState()
{
    m_state = NULL;
    m_stateSize = 0;
    
    assert(!s_devicesInited);

    m_memFile = qemu_memfile_open(s2e_qemu_get_buffer, s2e_qemu_put_buffer);


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

    if (!s_devicesInited) {
        void *se;
        g_s2e->getDebugStream() << "Looking for relevant virtual devices...";

        //Register all active devices detected by QEMU
        for(se = s2e_qemu_get_first_se();
            se != NULL; se = s2e_qemu_get_next_se(se)) {
                std::string deviceId(s2e_qemu_get_se_idstr(se));
                if (!ignoreList.count(deviceId)) {
                    g_s2e->getDebugStream() << "   Registering device " << deviceId << '\n';
                    s_devices.push_back(se);
                } else {
                    g_s2e->getDebugStream() << "   Shared device " << deviceId << '\n';
                }
        }
        s_devicesInited = true;
    }

    if (!PersistentDiskWrites) {
        g_s2e->getMessagesStream() <<
                "WARNING!!! All writes to disk will be lost after shutdown." << '\n';
        __hook_bdrv_read = s2e_bdrv_read;
        __hook_bdrv_write = s2e_bdrv_write;
    }else {
        g_s2e->getMessagesStream() <<
                "WARNING!!! All disk writes will be SHARED across states! BEWARE OF CORRUPTION!" << '\n';
    }
}

int s2edev_dbg=0;

void S2EDeviceState::saveDeviceState()
{
    s_currentDeviceState = this;

    qemu_make_readable(m_memFile);

    //DPRINTF("Saving device state %p\n", this);
    /* Iterate through all device descritors and call
    * their snapshot function */
    for (vector<void*>::iterator it = s_devices.begin(); it != s_devices.end(); it++) {
        void *se = *it;
        //DPRINTF("%s ", s2e_qemu_get_se_idstr(se));
        s2e_qemu_save_state(m_memFile, se);
    }
    //DPRINTF("\n");
    qemu_fflush(m_memFile);
    s_currentDeviceState = NULL;
}

void S2EDeviceState::restoreDeviceState()
{
    assert(m_stateSize);
    assert(m_state);

    s_currentDeviceState = this;

    qemu_make_readable(m_memFile);
    //DPRINTF("Restoring device state %p\n", this);
    for (vector<void*>::iterator it = s_devices.begin(); it != s_devices.end(); it++) {
        void *se = *it;
        //DPRINTF("%s ", s2e_qemu_get_se_idstr(se));  
        s2e_qemu_load_state(m_memFile, se);
    }
    //DPRINTF("\n");
    s_currentDeviceState = NULL;
}


/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/

void S2EDeviceState::allocateBuffer(unsigned int Sz)
{
    if (!m_state) {
        m_state = (unsigned char *)malloc(Sz);
        if (!m_state) {
            cerr << "Cannot allocate memory for device state snapshot" << endl;
            exit(-1);
        }
        m_stateSize = Sz;
        return;
    }

    if (Sz >= m_stateSize) {
        /* Need to expand the buffer */
        m_stateSize = Sz;
        m_state = (unsigned char*)realloc(m_state, m_stateSize);
        if (!m_state) {
            cerr << "Cannot reallocate memory for device state snapshot" << endl;
            exit(-1);
        }
        return;
    }
}

int S2EDeviceState::putBuffer(const uint8_t *buf, int64_t pos, int size)
{
    if (pos + size > m_stateSize) {
        allocateBuffer(pos + size);
    }
    memcpy(&m_state[pos], buf, size);
    return size;
}

int S2EDeviceState::getBuffer(uint8_t *buf, int64_t pos, int size)
{
    assert(pos <= m_stateSize);
    int toCopy = pos + size <= m_stateSize ? size : m_stateSize - pos;
    memcpy(buf, &m_state[pos], toCopy);
    return toCopy;
}


/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/

int S2EDeviceState::writeSector(struct BlockDriverState *bs, int64_t sector, const uint8_t *buf, int nb_sectors)
{
    SectorMap &dev = m_blockDevices[bs];
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


int S2EDeviceState::readSector(struct BlockDriverState *bs, int64_t sector, uint8_t *buf, int nb_sectors)
{
    bool hasRead = false;
    int readCount = 0;

  //  DPRINTF("readSector %#"PRIx64" count=%d\n", sector, nb_sectors);
    for (int64_t i = sector; i<sector+nb_sectors; i++) {
        for (S2EDeviceState *curState = this; curState; curState = curState->m_parent) {
            SectorMap &dev = curState->m_blockDevices[bs];
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
            break;
        }

        hasRead = false;
        ++readCount;
    }
    return readCount;
}

/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/

/**
*  Functions facing QEMU. They simmply forward the call to the right
*  device state.
*/

extern "C" {

int s2e_bdrv_read(struct BlockDriverState *bs, int64_t sector_num,
                  uint8_t *buf, int nb_sectors)
{
    S2EDeviceState *devState = g_s2e_state->getDeviceState();
    return devState->readSector(bs, sector_num, buf, nb_sectors);
}

int s2e_bdrv_write(BlockDriverState *bs, int64_t sector_num,
                   const uint8_t *buf, int nb_sectors
                   )
{
    S2EDeviceState *devState = g_s2e_state->getDeviceState();
    
    return devState->writeSector(bs, sector_num, buf, nb_sectors);

}

}
