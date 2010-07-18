#ifndef S2E_PLUGINS_SYMBHW_H
#define S2E_PLUGINS_SYMBHW_H

#include <s2e/Plugin.h>
#include <s2e/Plugins/CorePlugin.h>
#include <s2e/S2EExecutionState.h>
#include <s2e/ConfigFile.h>

#include <string>
#include <set>
#include <map>

namespace s2e {
namespace plugins {

class SymbolicHardware;

class DeviceDescriptor {
    std::string m_id;


public:
    DeviceDescriptor(const std::string &id);

    static DeviceDescriptor *create(SymbolicHardware *plg, ConfigFile *cfg, const std::string &key);
    virtual ~DeviceDescriptor();

    bool operator()(const DeviceDescriptor *dd) {
        return m_id < dd->m_id;
    }
};

class IsaDeviceDescriptor:public DeviceDescriptor {
public:
    struct IsaResource {
        uint16_t portBase;
        uint16_t portSize;
    };

private:
    IsaResource m_isaResource;

public:
    IsaDeviceDescriptor(const std::string &id, const IsaResource &res);

    static IsaDeviceDescriptor* create(SymbolicHardware *plg, ConfigFile *cfg, const std::string &key);
    virtual ~IsaDeviceDescriptor();
};

class PciDeviceDescriptor:public DeviceDescriptor {
public:
    struct PciResource{
        bool isIo;
        uint32_t size;
        bool prefetchable;
    };

private:
    uint16_t m_vid;
    uint16_t m_pid;
    uint32_t m_classCode;
    uint8_t m_revisionId;

    std::vector<PciResource> m_resources;

    PciDeviceDescriptor(const std::string &id):DeviceDescriptor(id) {};
public:
    static PciDeviceDescriptor* create(SymbolicHardware *plg, ConfigFile *cfg, const std::string &key);

};

class SymbolicHardware : public Plugin
{
    S2E_PLUGIN
public:

    typedef std::set<DeviceDescriptor *> DeviceDescriptors;


public:
    SymbolicHardware(S2E* s2e): Plugin(s2e) {}
    virtual ~SymbolicHardware();
    void initialize();

private:
    DeviceDescriptors m_devices;

};

} // namespace plugins
} // namespace s2e

#endif // S2E_PLUGINS_EXAMPLE_H
