#ifndef _DATA_STRUCTURE_SPY_

#define _DATA_STRUCTURE_SPY_

#include <string>
#include <set>

typedef struct _SProcessDescriptor
{
  uint64_t PageDirectory;
  std::string Name;
}SProcessDescriptor;

struct IDataStructureSpy
{
  /*********************************************/
  struct ProcessByDir {
    bool operator()(const struct _SProcessDescriptor* s1, 
      const struct _SProcessDescriptor* s2) const {
      return s1->PageDirectory < s2->PageDirectory;
    }
  };

  struct ProcessByName {
    bool operator()(const struct _SProcessDescriptor* s1, 
      const struct _SProcessDescriptor* s2) const {
      return s1->Name < s2->Name;
    }
  };

  /*********************************************/

public:
  typedef std::set<SProcessDescriptor*, ProcessByName> ProcessesByName;
  typedef std::set<SProcessDescriptor*, ProcessByDir> ProcessesByDir;

protected:
  

  virtual bool AddProcess(const SProcessDescriptor &Process) = 0;
  virtual void ClearProcesses() = 0;
public:
  
  virtual bool ScanProcesses()=0;
  virtual const SProcessDescriptor* FindProcess(const std::string &Name)=0;
  virtual const SProcessDescriptor* FindProcess(uint64_t cr3)=0;
  virtual bool GetCurrentThreadStack(void *State,
    uint64_t *StackTop, uint64_t *StackBottom)=0;
};



#endif
