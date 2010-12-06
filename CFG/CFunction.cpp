#include "llvm/Module.h"
#include "llvm/ModuleProvider.h"
#include "llvm/PassManager.h"
#include "llvm/Analysis/Verifier.h"
#include "llvm/target/TargetData.h"

#include "Passes/FunctionBuilder.h"

#include <sstream>
#include <iostream>

#include "CFunction.h"

using namespace llvm;

namespace s2etools {
namespace translator {

void CFunction::generate(FunctionAddressMap &fcnAddrMap)
{
    std::stringstream functionName;
    functionName << "function_" << std::hex << m_entryPoint->getAddress();
    FunctionBuilder builder(m_entryPoint->getFunction(),
                            fcnAddrMap,
                            functionName.str());

    builder.runOnModule(*m_entryPoint->getFunction()->getParent());
    m_function = builder.getFunction();

    std::cerr << *m_function << std::endl;

    //Check the correctness of the generated code
    valid();

}

void CFunction::valid()
{
    if (!m_function) {
        return;
    }

    //Check that the function is well-formed LLVM
    ExistingModuleProvider *MP = new ExistingModuleProvider(m_function->getParent());
    TargetData *TD = new TargetData(m_function->getParent());
    FunctionPassManager FcnPasses(MP);
    FcnPasses.add(TD);

    FcnPasses.add(createVerifierPass());
    FcnPasses.run(*m_function);
}

}
}
