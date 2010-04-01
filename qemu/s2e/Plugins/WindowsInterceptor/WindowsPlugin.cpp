#include "WindowsOS.h"
#include "UserModeInterceptor.h"

#include <string>
#include <cstring>
#include <iostream>
#include <assert.h>

using namespace std;

WindowsPlugin::WindowsPlugin(S2E* s2e) : Plugin(s2e)
{
  std::string OS = s2e->getConfig()->getString("guestOS.type");
  if (stricmp(OS.c_str(), "WINDOWS")) {
    std::cout << "Invalid guest type (" << OS << "). Must be Windows." << std::endl;
    exit(-1);
  }

  std::string Version = s2e->getConfig()->getString("guestOS.version");
  if (!stricmp(Version.c_str(), "SP2")) {
    m_Version = CWindowsOS::SP2;
  }else if (!stricmp(Version.c_str(), "SP3")) {
    m_Version = CWindowsOS::SP3;
  }else {
    std::cout << "Unsupported of invalid Windows version " << Version << std::endl;
  }

  switch(m_Version) {
    case SP2: m_KernelBase = 0x800d7000; break;
    case SP3: m_KernelBase = 0x800d7000; break;
    default: break;
  }
  m_PointerSize = 4;
  m_Spy = new WindowsSpy(this);
}


WindowsPlugin::~WindowsPlugin()
{
  delete m_Spy;
}

bool WindowsPlugin::CheckDriverLoad(uintptr_t eip) const
{
  switch(m_Version) {
    case SP2: return eip >= 0x805A078C && eip <= 0x805A0796;
    case SP3: return eip >= 0x805A3990 && eip <= 0x805A399A;
    default: return false;
  }
  return true;
}

bool WindowsPlugin::CheckPanic(uintptr_t eip) const
{
  eip -= m_KernelBase;

  switch(m_Version) {
    case SP2: return eip == 0x0045B7BA  || eip == 0x0045C2DF || eip == 0x0045C303;
    case SP3: return eip == 0x0045BCAA  || eip == 0x0045C7CD || eip == 0x0045C7F3;
    default: return false;
  }
  return true;
}

#if 0
uintptr_t CWindowsOS::GetDriverObject(void *cpu)
{
  target_uintptr_t StackPtr, ptr;

  switch(m_Version) {
    case SP2: StackPtr =  ((CPUState*)cpu)->regs[R_EBP] - 0x80; break;
    case SP3: StackPtr = ((CPUState*)cpu)->regs[R_EBP] - 0x80; break;
    default: assert(false && "Unknown OS"); break;
  }
 
  if (!QEMU::ReadVirtualMemory(StackPtr, &ptr, sizeof(ptr))) 
    return 0;
  return ptr;
}
#endif

uint64_t WindowsPlugin::GetUserAddressSpaceSize() const
{
  return 0x80000000;
}

uint64_t WindowsPlugin::GetKernelStart() const
{
  return 0x80000000;
}

uint64_t WindowsPlugin::GetLdrpCallInitRoutine() const
{
  switch(m_Version) {
    case SP2: return 0x7C901193;
    case SP3: return 0x7C901176;
  }
  assert(false && "Unknown OS version\n");
  return 0;
}

unsigned WindowsPlugin::GetPointerSize() const
{
  return m_PointerSize;
}

WindowsSpy *WindowsPlugin::GetSpy() const
{
  return m_Spy;
}