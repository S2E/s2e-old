#ifndef _S2E_DEVICE_STATE_H_

#define _S2E_DEVICE_STATE_H_

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

    static std::vector<void *> s_Devices;
    static std::set<std::string> s_customDevices;
    static bool s_DevicesInited;
    static S2EDeviceState *s_CurrentState;

    unsigned char *m_State;
    unsigned int m_StateSize;
    unsigned int m_Offset;


    static unsigned int s_PreferedStateSize;

    S2EDeviceState *m_Parent;
    BlockDeviceToSectorMap m_BlockDevices;
    bool  m_canTransferSector;
    

    void AllocateBuffer(unsigned int Sz);
    void ShrinkBuffer();

    void cloneDiskState();

    S2EDeviceState(const S2EDeviceState &);
public:

    static S2EDeviceState *getCurrentVmState() {
        return s_CurrentState;
    }

    bool canTransferSector() const;

    S2EDeviceState();
    void clone(S2EDeviceState **state1, S2EDeviceState **state2);
    ~S2EDeviceState();

    //From QEMU to KLEE
    void SaveDeviceState();
    
    //From KLEE to QEMU
    void RestoreDeviceState();

    void PutByte(int v);
    void PutBuffer(const uint8_t *buf, int size1);
    int GetByte();
    int GetBuffer(uint8_t *buf, int size1);

    int writeSector(struct BlockDriverState *bs, int64_t sector, const uint8_t *buf, int nb_sectors);
    int readSector(struct BlockDriverState *bs, int64_t sector, uint8_t *buf, int nb_sectors,
        s2e_raw_read fb);

    static void registerCustomDevice(const std::string &str) {
        s_customDevices.insert(str);
    }

    void initDeviceState();
    void restoreDeviceState();
    void saveDeviceState();
};

}

#endif
