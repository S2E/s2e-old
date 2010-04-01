#ifndef _WINDOWS_SPY_H_

#define _WINDOWS_SPY_H_

#include <s2e/Interceptor/DataStructureSpy.h>


namespace s2e {
namespace plugins {

class WindowsMonitor;

class WindowsSpy: public s2e::IDataStructureSpy
{
protected:

  static bool AddProcess(const SProcessDescriptor &ProcessOrig,
                            Processes &P);

  static void ClearProcesses(Processes &P);

  WindowsMonitor *m_OS;
public:

  WindowsSpy(WindowsMonitor *OS);

  typedef enum _EWindowsVersion {
    UNKNOWN, SP1, SP2, SP3
  }EWindowsVersion;

  virtual bool ScanProcesses(Processes &P);
  
  virtual bool FindProcess(const std::string &Name,
                             const IDataStructureSpy::Processes &P,
                             SProcessDescriptor &Result);
  virtual bool FindProcess(uint64_t cr3, 
    const IDataStructureSpy::Processes &P,
    SProcessDescriptor &Result);
  
  virtual bool GetCurrentThreadStack(void *State,
    uint64_t *StackTop, uint64_t *StackBottom);

  bool ReadUnicodeString(std::string &Result,uint64_t Offset);
  static EWindowsVersion GetVersion();
};

}
}

#endif

