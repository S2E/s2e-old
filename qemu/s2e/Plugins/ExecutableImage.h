#ifndef _IMAGE_H_

#define _IMAGE_H_


#include <map>
#include <iostream>

#include <s2e/S2EExecutionState.h>
#include "ModuleDescriptor.h"

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


public:

  virtual uint64_t GetBase() const = 0;
  virtual uint64_t GetImageBase() const = 0;
  virtual uint64_t GetImageSize() const = 0;
  virtual uint64_t GetEntryPoint() const = 0;
  virtual uint64_t GetRoundedImageSize() const = 0;

  virtual const Exports& GetExports(S2EExecutionState *s) = 0;

  virtual const Imports& GetImports(S2EExecutionState *s) = 0;

  virtual void DumpInfo(std::iostream &os) const = 0;

};



}
#endif


