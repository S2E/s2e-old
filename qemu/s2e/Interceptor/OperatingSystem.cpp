#include "OperatingSystem.h"
#include <s2e/S2E.h>
#include <s2e/Plugins/PluginInterface.h>
#include <s2e/Utils.h>

#include <iostream>
#include <stdlib.h>

using namespace std;



bool COperatingSystem::LoadModuleInterceptors()
{
    if (!m_Interface) {
        std::cout << "An OS plugin must be loaded setting module interceptors!" << std::endl;
        return false;
    }

#if 0
    vector<string> kernelInterceptors =
        m_s2e->config()->getStringList("interceptors.kernelMode");
    foreach(it, kernelInterceptors.begin(), kernelInterceptors.end()) {
        IInterceptor *I = m_Interface->GetNewInterceptor(*it, false);
        if (!I) {
            std::cout << "Could not create interceptor for " << (*it) << std::endl;
        }

        I->SetEventHandler(m_Events);
        m_Interceptors.push_back(I);
    }

    vector<string> userInterceptors =
        m_s2e->config()->getStringList("interceptors.userMode");
    foreach(it, userInterceptors.begin(), userInterceptors.end()) {
        IInterceptor *I = m_Interface->GetNewInterceptor(*it, true);
        if (!I) {
            std::cout << "Could not create interceptor for " << (*it) << std::endl;
        }

        I->SetEventHandler(m_Events);
        m_Interceptors.push_back(I);
    }
#endif

    string Module = m_s2e->config()->getString("interceptors[1].name");
    IInterceptor *I = m_Interface->GetNewInterceptor(Module, true);
    if (!I) {
        std::cout << "Could not create interceptor for " << Module << std::endl;
    }

    I->SetEventHandler(m_Events);
    m_Interceptors.push_back(I);

    return true;
}

bool COperatingSystem::IsLoaded() const
{
    return m_Loaded;
}

void COperatingSystem::SetInterface(IOperatingSystem *OS) {
    m_Interface = OS;
}

/////////////////////////////////////////////////////

COperatingSystem *COperatingSystem::s_Instance = NULL;

COperatingSystem::COperatingSystem(S2E *s2e)
{
    m_s2e = s2e;
    m_Interface = NULL;
    m_Loaded = Load();
    m_Events = new COSEvents(this);
}

COperatingSystem::~COperatingSystem()
{
    OSPLUGIN_RELEASE Inst = (OSPLUGIN_RELEASE)PluginInterface::GetEntryPoint(
        m_Plugin, "Release"); 

    if (!Inst) {
        std::cout << "Could not relase the OS plugin " << std::endl;
        return;
    }
    if (m_Interface) {
        Inst(m_Interface);
    }
    delete m_Events;
}


bool COperatingSystem::Load()
{
    string pluginPath;
    if(getenv("S2E_PLUGINPATH"))
        pluginPath = getenv("S2E_PLUGINPATH");
    string Plugin = PluginInterface::ConvertToFileName(
        pluginPath,
        m_s2e->config()->getString("guestOS.plugin"));

    void *LibHandle = PluginInterface::LoadPlugin(Plugin);
    if (!LibHandle) {
        std::cout << "Could not load " << Plugin << std::endl;
        return false;
    }

    OSPLUGIN_GETINSTANCE Inst = (OSPLUGIN_GETINSTANCE)PluginInterface::GetEntryPoint(
        LibHandle, "GetInstance");

    if (!Inst) {
        std::cout << "Could not find GetInstance entry point in " << Plugin << std::endl; 
        return false;
    }

    S2E_PLUGIN_API API;
    PluginInterface::PluginApiInit(API);

    string osType = m_s2e->config()->getString("guestOS.type");
    string osVersion = m_s2e->config()->getString("guestOS.version");

    IOperatingSystem *IOS = Inst(osType.c_str(), osVersion.c_str(), &API);

    if (IOS) {
        m_Interface = IOS;
        m_Plugin = LibHandle;
        std::cout << "Loaded plugin for " << osType
            << " " << osVersion << " - " << Plugin
            << std::endl;

        return true;
    }

    PluginInterface::UnloadPlugin(LibHandle);

    return false;
}


bool COperatingSystem::OnTbEnter(void *CpuState, bool Translation)
{
    foreach(it, m_Interceptors.begin(), m_Interceptors.end()) {
        if ((*it)->OnTbEnter(CpuState, Translation)) {
            return true;
        }
    }
    return false;
}

bool COperatingSystem::OnTbExit(void *CpuState, bool Translation)
{
    foreach(it, m_Interceptors.begin(), m_Interceptors.end()) {
        if ((*it)->OnTbExit(CpuState, Translation)) {
            return true;
        }
    }
    return false;
}


COSEvents::COSEvents(COperatingSystem *Os)
{

}

COSEvents::~COSEvents()
{

}

void COSEvents::OnProcessLoad(
struct IInterceptor *Interceptor,
    const ModuleDescriptor &Desc,
    const IExecutableImage::Imports &Imports,
    const IExecutableImage::Exports &Export
    )
{
    std::cout << "PROCESS LOAD ";
    Desc.Print(std::cout);
}

void COSEvents::OnLibraryLoad(
struct IInterceptor *Interceptor,
    const ModuleDescriptor &Desc,
    const IExecutableImage::Imports &Imports,
    const IExecutableImage::Exports &Export
    )
{
    std::cout << "LIBRARY LOAD ";
    Desc.Print(std::cout);
}
