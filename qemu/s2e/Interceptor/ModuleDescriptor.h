#ifndef _MODULE_DESCRIPTOR_H_

#define _MODULE_DESCRIPTOR_H_

#include <inttypes.h>
#include <string>
#include <set>
#include <ostream>

#include <cstring>

/**
 *  Characterizes whatever module can be loaded in the memory.
 *  This can be a user-mode library, or a kernel-mode driver.
 */
struct ModuleDescriptor
{
  //The name of the module (eg. MYAPP.EXE or DRIVER.SYS)
  std::string Name;
  
  //Where the the prefered load address of the module.
  //This is defined by the linker and put into the header of the image.
  uint64_t NativeBase;
  
  //Where the image of the module was actually loaded by the OS.
  uint64_t LoadBase;
  
  //The size of the image of the module
  uint64_t Size;

  bool Contains(uint64_t RunTimeAddress) const {
    uint64_t RVA = RunTimeAddress - LoadBase;
    return RVA < Size;
  }

  bool EqualInsensitive(const char *Name) const{
	return strcasecmp(this->Name.c_str(), Name) == 0;
  }

  struct ModuleByLoadBase {
    bool operator()(const struct ModuleDescriptor& s1, 
      const struct ModuleDescriptor& s2) const {
      return s1.LoadBase < s2.LoadBase;
    }
  };

  void Print(std::ostream &os) const {
    std::cout << "Name=" << Name << std::hex <<
      " NativeBase=0x" << NativeBase << " LoadBase=0x" << LoadBase <<
      " Size=0x" << Size << std::endl;
  };


  typedef std::set<struct ModuleDescriptor, ModuleByLoadBase> MDSet;
};

#endif
