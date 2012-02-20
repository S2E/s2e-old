/*
 * S2E Selective Symbolic Execution Framework
 *
 * Copyright (c) 2010, Dependable Systems Laboratory, EPFL
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Dependable Systems Laboratory, EPFL nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE DEPENDABLE SYSTEMS LABORATORY, EPFL BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Currently maintained by:
 *    Vitaly Chipounov <vitaly.chipounov@epfl.ch>
 *    Volodymyr Kuznetsov <vova.kuznetsov@epfl.ch>
 *
 * All contributors are listed in S2E-AUTHORS file.
 *
 */

extern "C"
{
#include "cpu.h"
#include "hw/hw.h"
#include "hw/pci.h"
#include "hw/isa.h"
}

#include "SymbolicHardware.h"
#include <s2e/S2E.h>
#include <s2e/S2EDeviceState.h>
#include <s2e/ConfigFile.h>
#include <s2e/Utils.h>

#include "llvm/Support/CommandLine.h"

#include <sstream>

extern struct CPUX86State *env;

namespace {
    //Allows bypassing the symbolic value injection.
    //All read accesses return concrete 0 values, and writes are ignored.
    llvm::cl::opt<bool>
    EnableSymbHw("s2e-enable-symbolic-hardware",
                     llvm::cl::init(true));
}

namespace s2e {
namespace plugins {

struct SymbolicPciDeviceState {
    PCIDevice dev;
    PciDeviceDescriptor *desc;
    MemoryRegion io[PCI_NUM_REGIONS];
};

struct SymbolicIsaDeviceState {
    ISADevice dev;
    IsaDeviceDescriptor *desc;
    qemu_irq qirq;
    MemoryRegion io;
};


extern "C" {
    static bool symbhw_is_symbolic(uint16_t port, void *opaque);
    static bool symbhw_is_symbolic_none(uint16_t port, void *opaque);

    static bool symbhw_is_mmio_symbolic(uint64_t physaddr, uint64_t size, void *opaque);
    static bool symbhw_is_mmio_symbolic_none(uint64_t physaddr, uint64_t size, void *opaque);

    static int pci_symbhw_init(PCIDevice *pci_dev);
    static int pci_symbhw_uninit(PCIDevice *pci_dev);
    static int isa_symbhw_init(ISADevice *dev);

#if 0
    static void symbhw_write8(void *opaque, uint32_t address, uint32_t data);
    static void symbhw_write16(void *opaque, uint32_t address, uint32_t data);
    static void symbhw_write32(void *opaque, uint32_t address, uint32_t data);

    static void symbhw_mmio_writeb(void *opaque, target_phys_addr_t addr, uint32_t val);
    static void symbhw_mmio_writew(void *opaque, target_phys_addr_t addr, uint32_t val);
    static void symbhw_mmio_writel(void *opaque, target_phys_addr_t addr, uint32_t val);
    static uint32_t symbhw_mmio_readb(void *opaque, target_phys_addr_t addr);
    static uint32_t symbhw_mmio_readw(void *opaque, target_phys_addr_t addr);
    static uint32_t symbhw_mmio_readl(void *opaque, target_phys_addr_t addr);
#endif
}


S2E_DEFINE_PLUGIN(SymbolicHardware, "Symbolic hardware plugin for PCI/ISA devices", "SymbolicHardware",);

void SymbolicHardware::initialize()
{
    ConfigFile *cfg = s2e()->getConfig();
    llvm::raw_ostream &ws = s2e()->getWarningsStream();
    bool ok;

    s2e()->getMessagesStream() << "======= Initializing Symbolic Hardware =======" << '\n';

    ConfigFile::string_list keys = cfg->getListKeys(getConfigKey(), &ok);
    if (!ok || keys.empty()) {
        ws << "No symbolic device descriptor specified in " << getConfigKey() << "." <<
                " S2E will start without symbolic hardware." << '\n';
        return;
    }

    foreach2(it, keys.begin(), keys.end()) {
        std::stringstream ss;
        ss << getConfigKey() << "." << *it;
        DeviceDescriptor *dd = DeviceDescriptor::create(this, cfg, ss.str());
        if (!dd) {
            ws << "Failed to create a symbolic device for " << ss.str() << '\n';
            exit(-1);
        }

        dd->print(s2e()->getMessagesStream());
        m_devices.insert(dd);
    }

    s2e()->getCorePlugin()->onDeviceRegistration.connect(
        sigc::mem_fun(*this, &SymbolicHardware::onDeviceRegistration)
    );

    s2e()->getCorePlugin()->onDeviceActivation.connect(
        sigc::mem_fun(*this, &SymbolicHardware::onDeviceActivation)
    );

    //Reset all symbolic bits for now
    memset(m_portMap, 0, sizeof(m_portMap));

    if (EnableSymbHw) {
        s2e()->getCorePlugin()->setPortCallback(symbhw_is_symbolic, this);
        s2e()->getCorePlugin()->setMmioCallback(symbhw_is_mmio_symbolic, this);
    }else {
        s2e()->getCorePlugin()->setPortCallback(symbhw_is_symbolic_none, this);
        s2e()->getCorePlugin()->setMmioCallback(symbhw_is_mmio_symbolic_none, this);
    }
}

//XXX: Do it per-state!
void SymbolicHardware::setSymbolicPortRange(uint16_t start, unsigned size, bool isSymbolic)
{
    assert(start + size <= 0x10000 && start+size>=start);
    for(uint16_t i = start; i<start+size; i++) {
        uint16_t idx = i/(sizeof(m_portMap[0])*8);
        uint16_t mod = i%(sizeof(m_portMap[0])*8);

        if (isSymbolic) {
            m_portMap[idx] |= 1<<mod;
        }else {
            m_portMap[idx] &= ~(1<<mod);
        }
    }
}

bool SymbolicHardware::isSymbolic(uint16_t port) const
{
    uint16_t idx = port/(sizeof(m_portMap[0])*8);
    uint16_t mod = port%(sizeof(m_portMap[0])*8);
    return m_portMap[idx] & (1<<mod);
}

//This can be used in two cases:
//1: On device registration, to map the MMIO registers
//2: On DMA memory registration, in conjunction with the OS annotations.
bool SymbolicHardware::setSymbolicMmioRange(S2EExecutionState *state, uint64_t physaddr, uint64_t size)
{
    s2e()->getDebugStream() << "SymbolicHardware: adding MMIO range 0x" << hexval(physaddr)
            << " length=0x" << size << '\n';

    assert(state->isActive());

    DECLARE_PLUGINSTATE(SymbolicHardwareState, state);
    bool b = plgState->setMmioRange(physaddr, size, true);
    if (b) {
        //We must flush the TLB, so that the next access can be taken into account
        tlb_flush(state->getConcreteCpuState(), 1);
    }
    return b;
}

//XXX: report already freed ranges
bool SymbolicHardware::resetSymbolicMmioRange(S2EExecutionState *state, uint64_t physaddr, uint64_t size)
{
    DECLARE_PLUGINSTATE(SymbolicHardwareState, state);
    return plgState->setMmioRange(physaddr, size, 0);
}

bool SymbolicHardware::isMmioSymbolic(uint64_t physaddress, uint64_t size) const
{
    DECLARE_PLUGINSTATE_CONST(SymbolicHardwareState, g_s2e_state);

    bool b = plgState->isMmio(physaddress, size);
    //s2e()->getDebugStream() << "isMmioSymbolic: 0x" << std::hex << physaddress << " res=" << b << '\n';
    return b;
}

static bool symbhw_is_symbolic(uint16_t port, void *opaque)
{
    SymbolicHardware *hw = static_cast<SymbolicHardware*>(opaque);
    return hw->isSymbolic(port);
}

static bool symbhw_is_symbolic_none(uint16_t port, void *opaque)
{
    return false;
}

static bool symbhw_is_mmio_symbolic(uint64_t physaddr, uint64_t size, void *opaque)
{
    SymbolicHardware *hw = static_cast<SymbolicHardware*>(opaque);
    return hw->isMmioSymbolic(physaddr, size);
}

static bool symbhw_is_mmio_symbolic_none(uint64_t physaddr, uint64_t size, void *opaque)
{
    return false;
}

DeviceDescriptor *SymbolicHardware::findDevice(const std::string &name) const
{
    DeviceDescriptor dd(name);
    DeviceDescriptors::const_iterator it = m_devices.find(&dd);
    if (it != m_devices.end()) {
        return *it;
    }
    return NULL;
}

void SymbolicHardware::onDeviceRegistration()
{
    s2e()->getMessagesStream() << "Registering symbolic devices with QEMU..." << '\n';
    foreach2(it, m_devices.begin(), m_devices.end()) {
        (*it)->initializeQemuDevice();
    }
}

void SymbolicHardware::onDeviceActivation(struct PCIBus* pci)
{
    s2e()->getMessagesStream() << "Activating symbolic devices..." << '\n';
    foreach2(it, m_devices.begin(), m_devices.end()) {
        (*it)->activateQemuDevice(pci);
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
   m_qemuIrq = NULL;
   m_qemuDev = NULL;
}

DeviceDescriptor::~DeviceDescriptor()
{

}

DeviceDescriptor *DeviceDescriptor::create(SymbolicHardware *plg, ConfigFile *cfg, const std::string &key)
{
    bool ok;
    llvm::raw_ostream &ws = plg->s2e()->getWarningsStream();

    std::string id = cfg->getString(key + ".id", "", &ok);
    if (!ok || id.empty()) {
        ws << "You must specifiy an id for " << key << ". " <<
                "This is required by QEMU for saving/restoring snapshots." << '\n';
        return NULL;
    }

    //Check the type of device we want to create
    std::string devType = cfg->getString(key + ".type", "", &ok);
    if (!ok || (devType != "pci" && devType != "isa")) {
        ws << "You must define either an ISA or PCI device!" << '\n';
        return NULL;
    }

    if (devType == "isa") {
        return IsaDeviceDescriptor::create(plg, cfg, key);
    }else if (devType == "pci") {
        return PciDeviceDescriptor::create(plg, cfg, key);
    }

    return NULL;
}

/////////////////////////////////////////////////////////////////////
IsaDeviceDescriptor::IsaDeviceDescriptor(const std::string &id, const IsaResource &res):DeviceDescriptor(id) {
    m_isaResource = res;
    m_isaInfo = NULL;
    m_isaProperties = NULL;
}

void IsaDeviceDescriptor::initializeQemuDevice()
{
    m_isaInfo = new ISADeviceInfo();
    m_isaProperties = new Property[1];
    memset(m_isaProperties, 0, sizeof(Property));

    m_isaInfo->qdev.name = m_id.c_str();
    m_isaInfo->qdev.size = sizeof(SymbolicIsaDeviceState);
    m_isaInfo->init = isa_symbhw_init;
    m_isaInfo->qdev.props = m_isaProperties;

    isa_qdev_register(m_isaInfo);
}

void IsaDeviceDescriptor::activateQemuDevice(struct PCIBus *bus)
{
    isa_create_simple(m_id.c_str());
    if (!isActive()) {
        g_s2e->getWarningsStream() << "ISA device " <<
                m_id << " is not active. Check that its ID does not collide with native QEMU devices." << '\n';
        exit(-1);
    }
}

IsaDeviceDescriptor::~IsaDeviceDescriptor()
{
    if (m_isaInfo) {
        delete m_isaInfo;
    }

    if (m_isaProperties) {
        delete [] m_isaProperties;
    }
}

void IsaDeviceDescriptor::print(llvm::raw_ostream &os) const
{
    os << "ISA Device Descriptor id=" << m_id << '\n';
    os << "Base=" << hexval(m_isaResource.portBase) <<
            " Size=" << hexval(m_isaResource.portSize) << '\n';
    os << '\n';
}

IsaDeviceDescriptor* IsaDeviceDescriptor::create(SymbolicHardware *plg, ConfigFile *cfg, const std::string &key)
{
    bool ok;
    llvm::raw_ostream &ws = plg->s2e()->getWarningsStream();

    std::string id = cfg->getString(key + ".id", "", &ok);
    assert(ok);

    uint64_t start = cfg->getInt(key + ".start", 0, &ok);
    if (!ok || start > 0xFFFF) {
        ws << "The base address of an ISA device must be between 0x0 and 0xffff." << '\n';
        return NULL;
    }

    uint16_t size = cfg->getInt(key + ".size", 0, &ok);
    if (!ok) {
        return NULL;
    }

    if (start + size > 0x10000) {
        ws << "An ISA address range must not exceed 0xffff." << '\n';
        return NULL;
    }

    uint8_t irq =  cfg->getInt(key + ".irq", 0, &ok);
    if (!ok || irq > 15) {
        ws << "You must specify an IRQ between 0 and 15 for the ISA device." << '\n';
        return NULL;
    }

    IsaResource r;
    r.portBase = start;
    r.portSize = size;
    r.irq = irq;

    return new IsaDeviceDescriptor(id, r);
}

void IsaDeviceDescriptor::setInterrupt(bool state)
{
    g_s2e->getDebugStream() << "IsaDeviceDescriptor::setInterrupt " << state << '\n';
    assert(m_qemuIrq);
    if (state) {
       qemu_irq_raise(*(qemu_irq*)m_qemuIrq);
    }else {
       qemu_irq_lower(*(qemu_irq*)m_qemuIrq);
    }
}

void IsaDeviceDescriptor::assignIrq(void *irq)
{
    m_qemuIrq = irq;
}

/////////////////////////////////////////////////////////////////////

PciDeviceDescriptor* PciDeviceDescriptor::create(SymbolicHardware *plg, ConfigFile *cfg, const std::string &key)
{
    bool ok;
    llvm::raw_ostream &ws = plg->s2e()->getWarningsStream();

    std::string id = cfg->getString(key + ".id", "", &ok);
    assert(ok);

    uint16_t vid = cfg->getInt(key + ".vid", 0, &ok);
    if (!ok) {
        ws << "You must specifiy a vendor id for a symbolic PCI device!" << '\n';
        return NULL;
    }

    uint16_t pid = cfg->getInt(key + ".pid", 0, &ok);
    if (!ok) {
        ws << "You must specifiy a product id for a symbolic PCI device!" << '\n';
        return NULL;
    }

    uint32_t classCode = cfg->getInt(key + ".classCode", 0, &ok);
    if (!ok || classCode > 0xffffff) {
        ws << "You must specifiy a valid class code for a symbolic PCI device!" << '\n';
        return NULL;
    }

    uint8_t revisionId = cfg->getInt(key + ".revisionId", 0, &ok);
    if (!ok) {
        ws << "You must specifiy a revision id for a symbolic PCI device!" << '\n';
        return NULL;
    }

    uint8_t interruptPin = cfg->getInt(key + ".interruptPin", 0, &ok);
    if (!ok || interruptPin > 4) {
        ws << "You must specifiy an interrupt pin (1-4, 0 for none) for " << key << "!" << '\n';
        return NULL;
    }

    std::vector<PciResource> resources;

    //Reading the resource list
    ConfigFile::string_list resKeys = cfg->getListKeys(key + ".resources", &ok);
    if (!ok || resKeys.empty()) {
        ws << "You must specifiy at least one resource descriptor for a symbolic PCI device!" << '\n';
        return NULL;
    }

    foreach2(it, resKeys.begin(), resKeys.end()) {
        std::stringstream ss;
        ss << key << ".resources." << *it;

        bool isIo = cfg->getBool(ss.str() + ".isIo", false, &ok);
        if (!ok) {
            ws << "You must specify whether the resource " << ss.str() << " is IO or MMIO!" << '\n';
            return NULL;
        }

        bool isPrefetchable = cfg->getBool(ss.str() + ".isPrefetchable", false, &ok);
        if (!ok && !isIo) {
            ws << "You must specify whether the resource " << ss.str() << " is prefetchable!" << '\n';
            return NULL;
        }

        uint32_t size = cfg->getInt(ss.str() + ".size", 0, &ok);
        if (!ok) {
            ws << "You must specify a size for the resource " << ss.str() << "!" << '\n';
            return NULL;
        }

        PciResource res;
        res.isIo = isIo;
        res.prefetchable = isPrefetchable;
        res.size = size;
        resources.push_back(res);
    }

    if (resources.size() > 6) {
        ws << "A PCI device can have at most 6 resource descriptors!" << '\n';
        return NULL;
    }

    PciDeviceDescriptor *ret = new PciDeviceDescriptor(id);
    ret->m_classCode = classCode;
    ret->m_pid = pid;
    ret->m_vid = vid;
    ret->m_revisionId = revisionId;
    ret->m_interruptPin = interruptPin;
    ret->m_resources = resources;

    return ret;
}


void PciDeviceDescriptor::initializeQemuDevice()
{
    g_s2e->getDebugStream() << "PciDeviceDescriptor::initializeQemuDevice()" << '\n';
    m_vmStateFields = new VMStateField[2];
    memset(m_vmStateFields, 0, sizeof(VMStateField)*2);
    m_vmStateFields[0].name = "dev";
    m_vmStateFields[0].size = sizeof(PCIDevice);
    m_vmStateFields[0].vmsd = &vmstate_pci_device;
    m_vmStateFields[0].flags = VMS_STRUCT;
    m_vmStateFields[0].offset = vmstate_offset_value(SymbolicPciDeviceState, dev, PCIDevice);

    m_vmState = new VMStateDescription();
    memset(m_vmState, 0, sizeof(VMStateDescription));

    m_vmState->name = m_id.c_str();
    m_vmState->version_id = 3,
    m_vmState->minimum_version_id = 3,
    m_vmState->minimum_version_id_old = 3,
    m_vmState->fields = m_vmStateFields;


    m_pciInfo = new PCIDeviceInfo();
    m_pciInfo->qdev.name = m_id.c_str();
    m_pciInfo->qdev.size = sizeof(SymbolicPciDeviceState);
    m_pciInfo->qdev.vmsd = m_vmState;
    m_pciInfo->init = pci_symbhw_init;
    m_pciInfo->exit = pci_symbhw_uninit;

    m_pciInfoProperties = new Property[1];
    memset(m_pciInfoProperties, 0, sizeof(Property));

    m_pciInfo->qdev.props = m_pciInfoProperties;
    pci_qdev_register(m_pciInfo);
}

void PciDeviceDescriptor::activateQemuDevice(struct PCIBus *bus)
{
    void *res = pci_create_simple(bus, -1, m_id.c_str());
    assert(res);

    if (!isActive()) {
        g_s2e->getWarningsStream() << "PCI device " <<
                m_id << " is not active. Check that its ID does not collide with native QEMU devices." << '\n';
        exit(-1);
    }
}

bool PciDeviceDescriptor::readPciAddressSpace(void *buffer, uint32_t offset, uint32_t size)
{
    PCIDevice *pci = (PCIDevice*)m_qemuDev;
    assert(pci);

    if (offset + size > 256) {
        return false;
    }

    memcpy(buffer, pci->config + offset, size);
    return true;
}

PciDeviceDescriptor::PciDeviceDescriptor(const std::string &id):DeviceDescriptor(id)
{
    m_pciInfo = NULL;
    m_pciInfoProperties = NULL;
    m_vmState = NULL;
}

PciDeviceDescriptor::~PciDeviceDescriptor()
{
    if (m_pciInfo) delete m_pciInfo;
    if (m_pciInfoProperties) delete [] m_pciInfoProperties;
    if (m_vmState) delete m_vmState;
}

void PciDeviceDescriptor::print(llvm::raw_ostream &os) const
{
    os << "PCI Device Descriptor id=" << m_id << '\n';
    os << "VID=" << hexval(m_vid) <<
            " PID=0x" << m_pid <<
            " RevID=" << hexval(m_revisionId) << '\n';

    os << "Class=" << hexval(m_classCode) <<
            " INT=0x" << hexval(m_interruptPin) << '\n';

    unsigned i=0;
    foreach2(it, m_resources.begin(), m_resources.end()) {
        const PciResource &res = *it;
        os << "R[" << i << "]: " <<
                "Size=0x" << res.size << " IsIO=" << (int)res.isIo <<
                " IsPrefetchable=" << hexval(res.prefetchable) << '\n';
    }
    os << '\n';
}

void PciDeviceDescriptor::setInterrupt(bool state)
{
    g_s2e->getDebugStream() << "PciDeviceDescriptor::setInterrupt " << state << '\n';
    assert(m_qemuIrq);
    if (state) {
       //s2e_print_apic(env);
        qemu_irq_raise(*(qemu_irq*)m_qemuIrq);
       // s2e_print_apic(env);
    }else {
        //s2e_print_apic(env);
       qemu_irq_lower(*(qemu_irq*)m_qemuIrq);
       //s2e_print_apic(env);
    }
}

void PciDeviceDescriptor::assignIrq(void *irq)
{
    m_qemuIrq = (qemu_irq*)irq;
}

/////////////////////////////////////////////////////////////////////
/* Dummy I/O functions for symbolic devices. Unused for now. */

/////////////////////////////////////////////////////////////////////
/* Dummy I/O functions for symbolic devices. Unused for now. */
static uint64_t symbhw_read(void *opaque, target_phys_addr_t addr,
                            unsigned size)
{
    return 0;
}

static void symbhw_write(void *opaque, target_phys_addr_t addr,
                         uint64_t data, unsigned size)
{

}

static const MemoryRegionOps symbhw_io_ops = {
    .read = symbhw_read,
    .write = symbhw_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};


/////////////////////////////////////////////////////////////////////

static int isa_symbhw_init(ISADevice *dev)
{
    g_s2e->getDebugStream() << __FUNCTION__ << " called" << '\n';

    SymbolicIsaDeviceState *isa = DO_UPCAST(SymbolicIsaDeviceState, dev, dev);

    SymbolicHardware *hw = (SymbolicHardware*)g_s2e->getPlugin("SymbolicHardware");
    assert(hw);

    IsaDeviceDescriptor *dd = (IsaDeviceDescriptor*)hw->findDevice(dev->qdev.info->name);
    assert(dd);

    isa->desc = dd;
    dd->setActive(true);
    dd->setDevice(dev);

    IsaDeviceDescriptor *s = isa->desc;

    uint32_t size = s->getResource().portSize;
    uint32_t addr = s->getResource().portBase;
    uint32_t irq = s->getResource().irq;

    std::stringstream ss;
    ss << "fakeisa-" << dev->qdev.info->name;

    memory_region_init_io(&isa->io, &symbhw_io_ops, isa, ss.str().c_str(), size);
    isa_register_ioport(dev, &isa->io, addr);

    hw->setSymbolicPortRange(addr, size, true);

    isa_init_irq(dev, &isa->qirq, irq);
    dd->assignIrq(&isa->qirq);
    return 0;
}


static int pci_symbhw_init(PCIDevice *pci_dev)
{
    SymbolicPciDeviceState *d = DO_UPCAST(SymbolicPciDeviceState, dev, pci_dev);
    uint8_t *pci_conf;

    s2e_debug_print("pci_symbhw_init\n");

    //Retrive the configuration
    SymbolicHardware *hw = (SymbolicHardware*)g_s2e->getPlugin("SymbolicHardware");
    assert(hw);

    PciDeviceDescriptor *dd = (PciDeviceDescriptor*)hw->findDevice(pci_dev->name);
    assert(dd);

    dd->setActive(true);

    d->desc = dd;
    dd->setDevice(&d->dev);

    pci_conf = d->dev.config;
    pci_config_set_vendor_id(pci_conf, dd->getVid());
    pci_config_set_device_id(pci_conf, dd->getPid());
    pci_config_set_class(pci_conf, dd->getClassCode());
    pci_conf[PCI_HEADER_TYPE] = PCI_HEADER_TYPE_NORMAL; // header_type
    pci_conf[0x3d] = dd->getInterruptPin(); // interrupt pin 0

    const PciDeviceDescriptor::PciResources &resources =
            dd->getResources();

    unsigned i=0;
    foreach2(it, resources.begin(), resources.end()) {
        const PciDeviceDescriptor::PciResource &res = *it;
        int type = 0;

        type |= res.isIo ? PCI_BASE_ADDRESS_SPACE_IO : PCI_BASE_ADDRESS_SPACE_MEMORY;
        type |= res.prefetchable ? PCI_BASE_ADDRESS_MEM_PREFETCH : 0;

        std::stringstream ss;
        ss << "fakepci-" << pci_dev->qdev.info->name;

        memory_region_init_io(&d->io[i], &symbhw_io_ops, d, ss.str().c_str(), res.size);
        pci_register_bar(&d->dev, i, type, &d->io[i]);

        ++i;
    }

    dd->assignIrq(&d->dev.irq[0]);

    return 0;
}

static int pci_symbhw_uninit(PCIDevice *pci_dev)
{
    SymbolicPciDeviceState *d = DO_UPCAST(SymbolicPciDeviceState, dev, pci_dev);

    cpu_unregister_io_memory(d->desc->mmio_io_addr);
    return 0;
}



//////////////////////////////////////////////////////////////
//Holds per-state information.


SymbolicHardwareState::SymbolicHardwareState()
{

}

SymbolicHardwareState::~SymbolicHardwareState()
{

}

SymbolicHardwareState* SymbolicHardwareState::clone() const
{
    return new SymbolicHardwareState(*this);
}

PluginState *SymbolicHardwareState::factory(Plugin *p, S2EExecutionState *s)
{
    return new SymbolicHardwareState();
}

//Unmaps a previously allocated range.
//phybase must fall inside an existing range.
bool SymbolicHardwareState::setMmioRange(uint64_t physbase, uint64_t size, bool b)
{
    uint64_t addr = physbase;
    while(size > 0) {
        MemoryRanges::iterator it = m_MmioMemory.find(addr & ~0xFFF);
        if (it == m_MmioMemory.end()) {
            if (!b) {
                //No need to reset anything,
                //Go to the next page
                uint64_t leftover = 0x1000 - (addr & 0xFFF);
                addr += leftover;
                size -= leftover > size ? size : leftover;
                continue;
            }else {
                //Need to create a new page
                m_MmioMemory[addr & ~0xFFF] = PageBitmap();
                it = m_MmioMemory.find(addr & ~0xFFF);
            }
        }

        uint32_t offset = addr & 0xFFF;
        uint32_t mysize = offset + size > 0x1000 ? 0x1000 - offset : size;

        bool fc = (*it).second.set(offset, mysize, b);
        if (fc) {
            //The entire page is concrete, do not need to keep it in the map
            m_MmioMemory.erase(addr & ~0xFFF);
        }

        size -= mysize;
        addr += mysize;
    }
    return true;
}

bool SymbolicHardwareState::isMmio(uint64_t physaddr, uint64_t size) const
{
    while (size > 0) {
        MemoryRanges::const_iterator it = m_MmioMemory.find(physaddr & ~0xFFF);
        if (it == m_MmioMemory.end()) {
            uint64_t leftover = 0x1000 - (physaddr & 0xFFF);
            physaddr += leftover;
            size -= leftover > size ? size : leftover;
            continue;
        }

        if (((physaddr & 0xFFF) == 0) && size>=0x1000) {
            if ((*it).second.hasSymbolic()) {
                return true;
            }
            size-=0x1000;
            physaddr+=0x1000;
            continue;
        }

        bool b = (*it).second.get(physaddr & 0xFFF);
        if (b) {
            return true;
        }

        size--;
        physaddr++;
    }
    return false;
}

///////////////////////////////////////////////
bool SymbolicHardwareState::PageBitmap::hasSymbolic() const
{
    return !isFullyConcrete();
}

void SymbolicHardwareState::PageBitmap::allocateBitmap(klee::BitArray *source) {
    if (source) {
        bitmap = new klee::BitArray(*source, 0x1000);
    } else {
        bitmap = new klee::BitArray(0x1000, false);
    }
}


bool SymbolicHardwareState::PageBitmap::isFullySymbolic() const {
    return fullySymbolic;
}

bool SymbolicHardwareState::PageBitmap::isFullyConcrete() const {
    if (!fullySymbolic) {
        if (!bitmap) {
            return true;
        }
        return bitmap->isAllZeros(0x1000);
    }
    return false;
}

SymbolicHardwareState::PageBitmap::PageBitmap() {
    fullySymbolic = false;
    bitmap = NULL;
}

//Copy constructor when cloning the state
SymbolicHardwareState::PageBitmap::PageBitmap(const PageBitmap &b1) {
    fullySymbolic = b1.fullySymbolic;
    bitmap = NULL;
    if (b1.bitmap) {
        allocateBitmap(b1.bitmap);
    }
}

SymbolicHardwareState::PageBitmap::~PageBitmap() {
    if (bitmap) {
        delete bitmap;
    }
}

//Returns true if the resulting range is fully concrete
bool SymbolicHardwareState::PageBitmap::set(unsigned offset, unsigned length, bool b) {
    assert(offset <= 0x1000 && offset + length <= 0x1000);
    allocateBitmap(NULL);

    for (unsigned i=offset; i<offset + length; ++i) {
        bitmap->set(i, b);
    }

    fullySymbolic = bitmap->isAllOnes(0x1000);
    bool fc = false;
    if (fullySymbolic || (fc = bitmap->isAllZeros(0x1000))) {
        delete bitmap;
        bitmap = NULL;
        return fc;
    }
    return false;
}

bool SymbolicHardwareState::PageBitmap::get(unsigned offset) const {
    assert(offset < 0x1000);
    if (isFullySymbolic()) {
        return true;
    }

    if (isFullyConcrete()) {
        return true;
    }

    assert(bitmap);

    return bitmap->get(offset);
}


} // namespace plugins
} // namespace s2e
