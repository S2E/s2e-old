#ifndef _S2E_DEVICE_STATE_H_

#define _S2E_DEVICE_STATE_H_

#include <vector>

namespace s2e {

class S2EDeviceState {
private:
  static std::vector<void *> s_Devices;
  static bool s_DevicesInited;

  unsigned char *m_State;
  unsigned int m_StateSize;
  unsigned int m_Offset;

  static unsigned int s_PreferedStateSize;


  void AllocateBuffer(unsigned int Sz);
  void ShrinkBuffer();

public:

  S2EDeviceState();
  S2EDeviceState *clone();
  ~S2EDeviceState();

  void SaveDeviceState();
  void RestoreDeviceState();

  static void ScheduleInterrupt(bool set);
  static void SchedulePicInterrupt(bool set);

  void PutByte(int v);
  void PutBuffer(const uint8_t *buf, int size1);
  int GetByte();
  int GetBuffer(uint8_t *buf, int size1);

  void initDeviceState();
  void restoreDeviceState();
  void saveDeviceState();
};

}

#endif
