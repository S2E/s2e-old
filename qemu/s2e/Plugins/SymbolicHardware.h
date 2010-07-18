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
protected:
    std::string m_id;


public:
    DeviceDescriptor(const std::string &id);

    static DeviceDescriptor *create(SymbolicHardware *plg, ConfigFile *cfg, const std::string &key);
    virtual ~DeviceDescriptor();

    bool operator()(const DeviceDescriptor *dd) {
        return m_id < dd->m_id;
    }

    virtual void print(std::ostream &os) const {}
    virtual void initializeQemuDevice() {assert(false);}
};

class IsaDeviceDescriptor:public DeviceDescriptor {
public:
    struct IsaResource {
        uint16_t portBase;
        uint16_t portSize;
        uint8_t irq;        
    };

private:
    IsaResource m_isaResource;

    struct ISADeviceInfo *m_isaInfo;
    struct Property *m_isaProperties;
public:
    IsaDeviceDescriptor(const std::string &id, const IsaResource &res);

    static IsaDeviceDescriptor* create(SymbolicHardware *plg, ConfigFile *cfg, const std::string &key);
    virtual ~IsaDeviceDescriptor();
    virtual void print(std::ostream &os) const;
    virtual void initializeQemuDevice();

    const IsaResource& getResource() const {
        return m_isaResource;
    }
};

class PciDeviceDescriptor:public DeviceDescriptor {
public:
    struct PciResource{
        bool isIo;
        uint32_t size;
        bool prefetchable;
    };

    typedef std::vector<PciResource> PciResources;
private:
    uint16_t m_vid;
    uint16_t m_pid;
    uint32_t m_classCode;
    uint8_t m_revisionId;
    uint8_t m_interruptPin;
    PciResources m_resources;

    struct _PCIDeviceInfo *m_pciInfo;
    struct Property *m_pciInfoProperties;
    struct VMStateDescription *m_vmState;
    struct _VMStateField *m_vmStateFields;

    PciDeviceDescriptor(const std::string &id);
    virtual void print(std::ostream &os) const;

public:
    int mmio_io_addr;

    uint16_t getVid() const { return m_vid; }
    uint16_t getPid() const { return m_pid; }
    uint32_t getClassCode() const { return m_classCode; }
    uint8_t getRevisionId() const { return m_revisionId; }
    uint8_t getInterruptPin() const { return m_interruptPin; }

    const PciResources& getResources() const { return m_resources; }
    static PciDeviceDescriptor* create(SymbolicHardware *plg, ConfigFile *cfg, const std::string &key);

    virtual void initializeQemuDevice();

    virtual ~PciDeviceDescriptor();
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

    const DeviceDescriptor *findDevice(const std::string &name) const;
private:
    DeviceDescriptors m_devices;

    void onDeviceRegistration();

};

} // namespace plugins
} // namespace s2e

#endif // S2E_PLUGINS_EXAMPLE_H
