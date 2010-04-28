#ifndef __S2E_MODEL_HANDLERS__

#define __S2E_MODEL_HANDLERS__

#include <stdint.h>
#include <map>

#include <s2e/S2E.h>
#include <s2e/Configuration/ConfigurationManager.h>

typedef bool (*FunctionHookHandler)(S2E &sd, bool isEntry);

//Must allow for any combination of C++/LLVM handlers.
//Models must track parameters accross calls/returns.
//Models 
struct ModelDescriptor {
    char *Name;
    FunctionHookHandler Function;
    unsigned ParamCount;
    unsigned Flags;
    llvm::Function *Model;
    unsigned InternalOffset;
    char *ModuleName;
    int ReturnSize;
};

struct InstructionDescriptor
{
    unsigned Flags;
    char *ModuleName;
    unsigned InternalOffset;
    llvm::Function *Model;
};

/**
 *  Describes a run-time interception point.
 */
typedef struct _SProcessHookPoint
{
    uint64_t Pid; //ProcessID (e.g., page directory address)
    uint64_t Pc;  //Program counter

    bool operator()(const struct _SProcessHookPoint& s1, 
      const struct _SProcessHookPoint& s2) const {
          if (s1.Pid == s2.Pid) {
              return s1.Pc < s2.Pc;
          }
          return s1.Pid < s2.Pid;
    }
}SProcessHookPoint;

///////////////////////////////////////////////////////////////////////////////
/**
 *  Interface exported by a library implementing models.
 *  The library declares an array of model descriptors and
 *  returns its caracteristics to the S2E engine.
 *
 */
class IModel
{
public:
    virtual SModelDescriptor *getModelDescriptors() = 0;
    virtual unsigned getModelDescriptorsCount() = 0;
};

class IModelBundle
{
    
};

class IModelState
{
public:
    virtual IModelState *Clone() = 0;
    virtual void Destroy() = 0;
};

//Need to have a model identifier to store


class CModelHandlers;

typedef IModels* (*MODLES_GETINSTANCE)(CModelHandlers *Handlers);
typedef void (*MODELS_RELEASE)(IModels *OS);

///////////////////////////////////////////////////////////////////////////////

class CModelHandlers
{
public:

    //Specify a model for each interception point 
    typedef std::map<SProcessHookPoint, SModelDescriptor> ModelMap;
    typedef std::map<SModelDescriptor *, bool> EnabledModelsMap;
private:
    //Models currently activated
    ModelMap m_HookedFunctions;
    
    //Models currently enabled (read from cfg file)
    EnabledDescriptorsMap m_EnabledModels;
    
    //All models available in the model plugin
    SModelDescriptor *m_AvailableModels;
    unsigned m_AvailableModelsCount;

public:
    CModelHandlers(CConfigurationManager *CfgMgr);
    ~CModelHandlers();

    ///////////////////////////////////////////////////////////////////////////
    //These functions are used by the module interception
    //mechanism whenever it detects a library/process load
    
    //RunTimePc is the program counter of the entry point to intercept
    bool registerEntryPoint(const struct ModuleDescriptor &Md, uint64_t Pid,
        uint64_t RunTimePc);

    //RunTimePc is the program counter of the library call to intercept
    bool registerHandler(const struct ModuleDescriptor &Md, uint64_t Pid,
        uint64_t RunTimePc);

    ///////////////////////////////////////////////////////////////////////////

    
    //Invoked by S2E whenever a function or an entry point is called
    bool handleCall(S2E &sd, uint64_t RunTimePc,
        unsigned *ParamCount, SModelDescriptor **CalledHandler);

    //Invoked by S2E whenever a return from a function or an entry point 
    //previously called is executed
    bool handleReturn(QEMUKLEE &sd, uint64_t RunTimePc, SModelDescriptor *d);

};


class CModels
{
public:
  typedef std::map<SProcessHookPoint, SModelDescriptor> ModelMap;
  
}


class IModels
{
public:
  
}

class CNDISModels:public IModels
{
public:
  typedef void (CNDISModels::*Model)(bool IsEntry);

 
  bool MiniportInitialize(bool IsEntry);

  bool NdisAllocateWithTag(bool IsEntry);
};


class CRTL8139Models:public IModels
{
  //This class should be able to register for events from
  //CNDISModels
};

#endif
