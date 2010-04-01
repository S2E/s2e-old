#ifndef _IMAGE_H_

#define _IMAGE_H_

#include <map>
#include <iostream>

namespace s2e {

/**
 *  This class models an executable image loaded in
 *  virtual memory.
 *  This is an abstract class which must be subclassed
 *  by actual implementation for Windows PE, Linux ELF, etc...
 */
struct IExecutableImage
{
public:
  //Maps the name of the exported function to its actual address
  typedef std::map<std::string,uint64_t> Exports;
  
  //Maps the name of the function to its actual address
  typedef std::map<std::string, uint64_t> ImportedFunctions;
  
  //Maps the library name to the set of functions it exports
  typedef std::map<std::string, ImportedFunctions > Imports;


public:

  virtual uint64_t GetBase() const = 0;
  virtual uint64_t GetImageBase() const = 0;
  virtual uint64_t GetImageSize() const = 0;
  virtual uint64_t GetEntryPoint() const = 0; 
  virtual uint64_t GetRoundedImageSize() const = 0;

  virtual const Exports& GetExports() const = 0;

  virtual const Imports& GetImports() const = 0;
  
  virtual void DumpInfo(std::iostream &os) const = 0;

};

}

#endif
