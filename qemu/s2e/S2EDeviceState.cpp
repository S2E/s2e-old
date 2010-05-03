extern "C" {
#include <qemu-common.h>
#include <block.h>

    void vm_stop(int reason);
    void vm_start(void);

    int s2e_dev_snapshot_enable = 0;
}

#include "s2e_block.h"

#include <iostream>
#include <s2e/Utils.h>
#include <s2e/S2E.h>
#include "S2EDeviceState.h"
#include "S2EExecutionState.h"

using namespace s2e;
using namespace std;

unsigned int S2EDeviceState::s_PreferedStateSize = 0x1000;

std::vector<void *> S2EDeviceState::s_Devices;
bool S2EDeviceState::s_DevicesInited=false;


#define REGISTER_DEVICE(dev) { if (!strcmp(s2e_qemu_get_se_idstr(se), dev)) { s_Devices.push_back(se); }}

//This is assumed to be called on fork.
//At that time, we need to save the state of the VM to 
//be later restored.
S2EDeviceState* S2EDeviceState::clone()
{
    S2EDeviceState* ret = new S2EDeviceState(*this);
    ret->m_Parent = this;
    ret->saveDeviceState();
    return ret;
}

S2EDeviceState::S2EDeviceState()
{
    m_Parent = NULL;
    m_canTransferSector = true;
}

void S2EDeviceState::initDeviceState()
{
    m_State = NULL;
    m_StateSize = 0;
    
    assert(!s_DevicesInited);

    g_s2e->getMessagesStream() << "Initing initial device state." << std::endl <<
        "WARNING!!! All writes to disk will be lost after shutdown." << std::endl;

    if (!s_DevicesInited) {
        void *se;
        cout << "Looking for relevant virtual devices..." << endl;

        for(se = s2e_qemu_get_first_se(); 
            se != NULL; se = s2e_qemu_get_next_se(se)) {
                cout << "State ID=" << s2e_qemu_get_se_idstr(se) << endl;
                REGISTER_DEVICE("i8259")
                REGISTER_DEVICE("PCIBUS")
                REGISTER_DEVICE("I440FX")
                REGISTER_DEVICE("PIIX3")
                REGISTER_DEVICE("ioapic")
                REGISTER_DEVICE("apic")
                REGISTER_DEVICE("mc146818rtc")
                REGISTER_DEVICE("i8254")
                REGISTER_DEVICE("hpet")
                REGISTER_DEVICE("dma")
                REGISTER_DEVICE("piix4_pm")
                REGISTER_DEVICE("ps2kbd")
                REGISTER_DEVICE("ps2mouse")
                REGISTER_DEVICE("i2c_bus")
                REGISTER_DEVICE("timer")
                REGISTER_DEVICE("ide")

                //XXX:Check for ps2kbd, ps2mouse, i2c_bus, timer
                //ide
        }
        s_DevicesInited = true;
    }

    __hook_bdrv_read = s2e_bdrv_read;
    __hook_bdrv_write = s2e_bdrv_write;
    __hook_bdrv_aio_read = s2e_bdrv_aio_read;
    __hook_bdrv_aio_write = s2e_bdrv_aio_write;

    //saveDeviceState();
    //restoreDeviceState();
}

void S2EDeviceState::saveDeviceState()
{
    s2e_dev_snapshot_enable = 1;
    vm_stop(0);
    m_Offset = 0;

    DPRINTF("Saving device state\n");
    /* Iterate through all device descritors and call
    * their snapshot function */
    for (vector<void*>::iterator it = s_Devices.begin(); it != s_Devices.end(); it++) {
        unsigned o = m_Offset;
        void *se = *it;
        DPRINTF("%s ", s2e_qemu_get_se_idstr(se));
        s2e_qemu_save_state(se);
        DPRINTF("sz=%d - ", m_Offset - o);
    }
    DPRINTF("\n");

    ShrinkBuffer();
    s2e_dev_snapshot_enable = 0;
    vm_start();
}


void S2EDeviceState::restoreDeviceState()
{
    vm_stop(0);
    s2e_dev_snapshot_enable = 1;

    m_Offset = 0;

    DPRINTF("Restoring device state\n");
    for (vector<void*>::iterator it = s_Devices.begin(); it != s_Devices.end(); it++) {
        unsigned o = m_Offset;
        void *se = *it;
        DPRINTF("%s ", s2e_qemu_get_se_idstr(se));  
        s2e_qemu_load_state(se);
        DPRINTF("sz=%d - ", m_Offset - o);
    }
    //DPRINTF("\n");

    s2e_dev_snapshot_enable = 0;
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
    s->getDeviceState()->PutByte(v);
}

int s2e_qemu_get_byte(S2EExecutionState *s)
{
    return s->getDeviceState()->GetByte();
}

int s2e_qemu_get_buffer(S2EExecutionState *s, uint8_t *buf, int size1)
{
    return s->getDeviceState()->GetBuffer(buf, size1);
}

void s2e_qemu_put_buffer(S2EExecutionState *s, const uint8_t *buf, int size)
{
    s->getDeviceState()->PutBuffer(buf, size);
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
