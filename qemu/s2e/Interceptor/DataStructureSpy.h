#ifndef _DATA_STRUCTURE_SPY_

#define _DATA_STRUCTURE_SPY_

#include <string>
#include <set>
#include <inttypes.h>

typedef struct _SProcessDescriptor
{
  uint64_t PageDirectory;
  std::string Name;
}SProcessDescriptor;

struct IDataStructureSpy
{
  /*********************************************/
  struct ProcessByDir {
    bool operator()(const struct _SProcessDescriptor& s1, 
      const struct _SProcessDescriptor& s2) const {
      return s1.PageDirectory < s2.PageDirectory;
    }
  };

  struct ProcessByName {
    bool operator()(const struct _SProcessDescriptor& s1, 
      const struct _SProcessDescriptor& s2) const {
      return s1.Name < s2.Name;
    }
  };

  /*********************************************/

public:
  typedef std::multiset<SProcessDescriptor, ProcessByName> ProcessesByName;
  typedef std::set<SProcessDescriptor, ProcessByDir> ProcessesByDir;

  typedef struct _Processes {
    ProcessesByName ByName;
    ProcessesByDir ByDir;
  }Processes;
public:
  
  virtual bool ScanProcesses(Processes &P)=0;
  virtual bool FindProcess(const std::string &Name,
                             const IDataStructureSpy::Processes &P,
                             SProcessDescriptor &Result) = 0;
  virtual bool FindProcess(uint64_t cr3, 
    const IDataStructureSpy::Processes &P,
    SProcessDescriptor &Result) = 0;
  
  virtual bool GetCurrentThreadStack(void *State,
    uint64_t *StackTop, uint64_t *StackBottom)=0;
};



#endif
