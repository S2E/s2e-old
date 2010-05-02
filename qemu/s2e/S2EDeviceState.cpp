extern "C" {
#include <qemu-common.h>

void vm_stop(int reason);
void vm_start(void);

}

#include <iostream>
#include "S2EDeviceState.h"
#include "S2EExecutionState.h"

using namespace s2e;
using namespace std;

unsigned int S2EDeviceState::s_PreferedStateSize = 0x1000;

std::vector<void *> S2EDeviceState::s_Devices;
bool S2EDeviceState::s_DevicesInited=false;


#define REGISTER_DEVICE(dev) { if (!strcmp(se->idstr, dev)) { s_Devices.push_back(se); }}

S2EDeviceState* S2EDeviceState::clone()
{
    return new S2EDeviceState(*this);
}

S2EDeviceState::S2EDeviceState()
{
  m_State = NULL;
  m_StateSize = 0;
#if 0
  if (!s_DevicesInited) {
    SaveStateEntry *se;
    cout << "Looking for relevant virtual devices..." << endl;
    
    for(se = first_se; se != NULL; se = se->next) {
      cout << "State ID=" << se->idstr << endl;
      REGISTER_DEVICE("i8259")
      REGISTER_DEVICE("PCIBUS")
      REGISTER_DEVICE("I440FX")
      REGISTER_DEVICE("PIIX3")
      REGISTER_DEVICE("ioapic")
      REGISTER_DEVICE("apic")
      REGISTER_DEVICE("mc146818rtc")
      REGISTER_DEVICE("i8254")
      REGISTER_DEVICE("dma")
      REGISTER_DEVICE("piix4_pm")
    }
    s_DevicesInited = true;

    ScheduleInterrupt(false);
    
    //XXX:This has to be changed for interrupts
//    SelectEs1370();
  }

  SaveDeviceState(this);
#endif
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

/**
 *  Functions facing QEMU. They simmply forward the call to the right
 *  device state.
 */

extern "C" {

int s2e_dev_snapshot_enable = 0;

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

}
