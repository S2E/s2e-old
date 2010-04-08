#ifndef _MODULE_DESCRIPTOR_H_

#define _MODULE_DESCRIPTOR_H_

#include <inttypes.h>
#include <string>
#include <set>
#include <map>
#include <ostream>
#include <iostream>

#include <cstring>

namespace s2e {

//Maps the name of the exported function to its actual address
typedef std::map<std::string,uint64_t> Exports;
  
//Maps the name of the function to its actual address
typedef std::map<std::string, uint64_t> ImportedFunctions;
  
//Maps the library name to the set of functions it exports
typedef std::map<std::string, ImportedFunctions > Imports;

/**
 *  Characterizes whatever module can be loaded in the memory.
 *  This can be a user-mode library, or a kernel-mode driver.
 */
struct ModuleDescriptor
{
  uint64_t  Pid;
  
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
        if (s1.Pid == s2.Pid) {
          return s1.LoadBase < s2.LoadBase;
        }
        return s1.Pid < s2.Pid;
    }
  };

  void Print(std::ostream &os) const {
    std::cout << "Name=" << Name << std::hex <<
      " NativeBase=0x" << NativeBase << " LoadBase=0x" << LoadBase <<
      " Size=0x" << Size << std::endl;
  };


  typedef std::set<struct ModuleDescriptor, ModuleByLoadBase> MDSet;
};

}

#endif
