#include "SymbolicHardware.h"
#include <s2e/S2E.h>
#include <s2e/ConfigFile.h>
#include <s2e/Utils.h>

#include <sstream>


namespace s2e {
namespace plugins {

S2E_DEFINE_PLUGIN(SymbolicHardware, "Symbolic hardware plugin for PCI/ISA devices", "SymbolicHardware",);

void SymbolicHardware::initialize()
{
    ConfigFile *cfg = s2e()->getConfig();
    std::ostream &ws = s2e()->getWarningsStream();
    bool ok;

    ConfigFile::string_list keys = cfg->getListKeys(getConfigKey(), &ok);
    if (!ok || keys.empty()) {
        ws << "No symbolic device descriptor specified in " << getConfigKey() << "." <<
                " S2E will start without symbolic hardware." << std::endl;
        return;
    }

    foreach2(it, keys.begin(), keys.end()) {
        std::stringstream ss;
        ss << getConfigKey() << "." << *it;
        DeviceDescriptor *dd = DeviceDescriptor::create(this, cfg, ss.str());
        if (!dd) {
            ws << "Failed to create a symbolic device for " << ss.str() << std::endl;
            exit(-1);
        }

        m_devices.insert(dd);
    }
}

SymbolicHardware::~SymbolicHardware()
{
    foreach2(it, m_devices.begin(), m_devices.end()) {
        delete *it;
    }
}

DeviceDescriptor::DeviceDescriptor(const std::string &id){
    m_id = id;
}

DeviceDescriptor::~DeviceDescriptor()
{

}

DeviceDescriptor *DeviceDescriptor::create(SymbolicHardware *plg, ConfigFile *cfg, const std::string &key)
{
    bool ok;
    std::ostream &ws = plg->s2e()->getWarningsStream();

    std::string id = cfg->getString(key + ".id", "", &ok);
    if (!ok || id.empty()) {
        ws << "You must specifiy an id for " << key << ". " <<
                "This is required by QEMU for saving/restoring snapshots." << std::endl;
        return NULL;
    }

    //Check the type of device we want to create
    std::string devType = cfg->getString(key + ".type", "", &ok);
    if (!ok || (devType != "pci" && devType != "isa")) {
        ws << "You must define either an ISA or PCI device!" << std::endl;
        return NULL;
    }

    if (devType == "isa") {
        return IsaDeviceDescriptor::create(plg, cfg, key);
    }else if (devType == "pci") {
        return PciDeviceDescriptor::create(plg, cfg, key);
    }

    return NULL;
}

IsaDeviceDescriptor::IsaDeviceDescriptor(const std::string &id, const IsaResource &res):DeviceDescriptor(id) {
    m_isaResource = res;
}

IsaDeviceDescriptor::~IsaDeviceDescriptor()
{

}

IsaDeviceDescriptor* IsaDeviceDescriptor::create(SymbolicHardware *plg, ConfigFile *cfg, const std::string &key)
{
    bool ok;
    std::ostream &ws = plg->s2e()->getWarningsStream();

    std::string id = cfg->getString(key + ".id", "", &ok);
    assert(ok);

    uint64_t start = cfg->getInt(key + ".start", 0, &ok);
    if (!ok || start > 0xFFFF) {
        ws << "The base address of an ISA device must be between 0x0 and 0xffff." << std::endl;
        return NULL;
    }

    uint16_t size = cfg->getInt(key + ".size", 0, &ok);
    if (!ok) {
        return NULL;
    }

    if (start + size > 0x10000) {
        ws << "An ISA address range must not exceed 0xffff." << std::endl;
        return NULL;
    }

    IsaResource r;
    r.portBase = start;
    r.portSize = size;

    return new IsaDeviceDescriptor(id, r);
}

PciDeviceDescriptor* PciDeviceDescriptor::create(SymbolicHardware *plg, ConfigFile *cfg, const std::string &key)
{
    bool ok;
    std::ostream &ws = plg->s2e()->getWarningsStream();

    std::string id = cfg->getString(key + ".id", "", &ok);
    assert(ok);

    uint16_t vid = cfg->getInt(key + ".vid", 0, &ok);
    if (!ok) {
        ws << "You must specifiy a vendor id for a symbolic PCI device!" << std::endl;
        return NULL;
    }

    uint16_t pid = cfg->getInt(key + ".pid", 0, &ok);
    if (!ok) {
        ws << "You must specifiy a product id for a symbolic PCI device!" << std::endl;
        return NULL;
    }

    uint32_t classCode = cfg->getInt(key + ".classCode", 0, &ok);
    if (!ok || classCode > 0xffffff) {
        ws << "You must specifiy a valid class code for a symbolic PCI device!" << std::endl;
        return NULL;
    }

    uint8_t revisionId = cfg->getInt(key + ".revisionId", 0, &ok);
    if (!ok) {
        ws << "You must specifiy a revision id for a symbolic PCI device!" << std::endl;
        return NULL;
    }

    std::vector<PciResource> resources;

    //Reading the resource list
    ConfigFile::string_list resKeys = cfg->getListKeys(key + ".resources", &ok);
    if (!ok || resKeys.empty()) {
        ws << "You must specifiy at least one resource descriptor for a symbolic PCI device!" << std::endl;
        return NULL;
    }

    foreach2(it, resKeys.begin(), resKeys.end()) {
        std::stringstream ss;
        ss << key << ".resources." << *it;

        bool isIo = cfg->getBool(ss.str() + ".isIo", false, &ok);
        if (!ok) {
            ws << "You must specify whether the resource " << ss.str() << " is IO or MMIO!" << std::endl;
            return NULL;
        }

        bool isPrefetchable = cfg->getBool(ss.str() + ".isPrefetchable", false, &ok);
        if (!ok && !isIo) {
            ws << "You must specify whether the resource " << ss.str() << " is prefetchable!" << std::endl;
            return NULL;
        }

        uint32_t size = cfg->getInt(ss.str() + ".size", 0, &ok);
        if (!ok) {
            ws << "You must specify a size for the resource " << ss.str() << "!" << std::endl;
            return NULL;
        }

        PciResource res;
        res.isIo = isIo;
        res.prefetchable = isPrefetchable;
        res.size = size;
        resources.push_back(res);
    }

    if (resources.size() > 6) {
        ws << "A PCI device can have at most 6 resource descriptors!" << std::endl;
        return NULL;
    }

    PciDeviceDescriptor *ret = new PciDeviceDescriptor(id);
    ret->m_classCode = classCode;
    ret->m_pid = pid;
    ret->m_vid = vid;
    ret->m_revisionId = revisionId;
    ret->m_resources = resources;

    return ret;
}

} // namespace plugins
} // namespace s2e
