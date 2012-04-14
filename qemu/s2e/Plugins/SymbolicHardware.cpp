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
#include "hw/fakepci.h"
#include "hw/sysbus.h"
#include "qemu/object.h"
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

//XXX: This should be the same as PCIFakeState?
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

    static void pci_symbhw_class_init(ObjectClass *klass, void *data);
    static void isa_symbhw_class_init(ObjectClass *klass, void *data);
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

void SymbolicHardware::onDeviceActivation(int bus_type, void *bus)
{
    s2e()->getMessagesStream() << "Activating symbolic devices..." << '\n';
    foreach2(it, m_devices.begin(), m_devices.end()) {
        if (bus_type == PCI && (*it)->isPci()) {
            (*it)->activateQemuDevice(bus);
        }
        if (bus_type == ISA && (*it)->isIsa()) {
            (*it)->activateQemuDevice(bus);
        }
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

   m_devInfo = NULL;
   m_devInfoProperties = NULL;
   m_vmState = NULL;
   m_vmStateFields = NULL;
}

DeviceDescriptor::~DeviceDescriptor()
{
    if (m_devInfo)
        delete m_devInfo;

    if (m_devInfoProperties)
        delete [] m_devInfoProperties;

    if (m_vmState)
        delete m_vmState;

    if (m_vmStateFields)
        delete m_vmStateFields;
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
    g_s2e->getDebugStream() << "IsaDeviceDescriptor::initializeQemuDevice()" << '\n';

    static TypeInfo fakeisa_info = {
        /* The name is changed at registration time */
        .name          = m_id.c_str(),
        .parent        = TYPE_ISA_DEVICE,
        .instance_size = sizeof(SymbolicIsaDeviceState),
        .class_init    = isa_symbhw_class_init,
        .class_data    = this,
    };

    m_isaInfo = new TypeInfo(fakeisa_info);

    m_isaProperties = new Property[1];
    memset(m_isaProperties, 0, sizeof(Property)*1);

    /*
    static const VMStateDescription vmstate_isa_fake = {
        .name = "...",
        .version_id = 3,
        .minimum_version_id = 3,
        .minimum_version_id_old = 3,
        .fields      = (VMStateField []) {
            VMSTATE_END_OF_LIST()
        }
    };*/

    m_vmStateFields = new VMStateField[1];
    memset(m_vmStateFields, 0, sizeof(VMStateField)*1);

    m_vmState = new VMStateDescription();
    memset(m_vmState, 0, sizeof(VMStateDescription));

    m_vmState->name = m_id.c_str();
    m_vmState->version_id = 3,
    m_vmState->minimum_version_id = 3,
    m_vmState->minimum_version_id_old = 3,
    m_vmState->fields = m_vmStateFields;

    type_register_static(m_isaInfo);
}

void IsaDeviceDescriptor::activateQemuDevice(void *bus)
{
    isa_create_simple((ISABus*)bus, m_id.c_str());

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

    TypeInfo fakepci_info = {
        /* The name is changed at registration time */
        .name          = m_id.c_str(),

        .parent        = TYPE_PCI_DEVICE,
        .instance_size = sizeof(SymbolicPciDeviceState),
        .class_init    = pci_symbhw_class_init,
        .class_data    = this,
    };

    m_devInfo = new TypeInfo(fakepci_info);

    m_devInfoProperties = new Property[1];
    memset(m_devInfoProperties, 0, sizeof(Property));

    /*
    static  VMStateDescription vmstate_pci_fake = {
        .name = "fakepci",
        .version_id = 3,
        .minimum_version_id = 3,
        .minimum_version_id_old = 3,
        .fields      = (VMStateField []) {
            VMSTATE_PCI_DEVICE(dev, PCIFakeState),
            VMSTATE_END_OF_LIST()
        }
    }; */

    m_vmStateFields = new VMStateField[2];
    memset(m_vmStateFields, 0, sizeof(VMStateField)*2);
    //Replaces VMSTATE_PCI_DEVICE()
    m_vmStateFields[0].name = m_id.c_str();
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

    type_register_static(m_devInfo);
}

void PciDeviceDescriptor::activateQemuDevice(void *bus)
{
    void *res = pci_create_simple((struct PCIBus*)bus, -1, m_id.c_str());
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
    m_vid = 0;
    m_pid = 0;
    m_ss_id = 0;
    m_ss_vid = 0;
    m_classCode = 0;
    m_revisionId = 0;
    m_interruptPin = 0;
}

PciDeviceDescriptor::~PciDeviceDescriptor()
{

}

void PciDeviceDescriptor::print(llvm::raw_ostream &os) const
{
    os << "PCI Device Descriptor id=" << m_id << '\n';
    os << "VID=" << hexval(m_vid) <<
            " PID=" << hexval(m_pid) <<
            " RevID=" << hexval(m_revisionId) << '\n';

    os << "Class=" << hexval(m_classCode) <<
            " INT=" << hexval(m_interruptPin) << '\n';

    unsigned i=0;
    foreach2(it, m_resources.begin(), m_resources.end()) {
        const PciResource &res = *it;
        os << "R[" << i << "]: " <<
                "Size=" << hexval(res.size) << " IsIO=" << (int)res.isIo <<
                " IsPrefetchable=" << hexval(res.prefetchable) << '\n';
        ++i;
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
    s2e_debug_print("isa_symbhw_init\n");

    SymbolicIsaDeviceState *symb_isa_state = DO_UPCAST(SymbolicIsaDeviceState, dev, dev);

    SymbolicHardware *hw = static_cast<SymbolicHardware*>(g_s2e->getPlugin("SymbolicHardware"));
    assert(hw);

    const char *devName = object_class_get_name(dev->qdev.parent_obj.klass);
    IsaDeviceDescriptor *isa_device_desc = static_cast<IsaDeviceDescriptor*>(hw->findDevice(devName));
    assert(isa_device_desc);

    symb_isa_state->desc = isa_device_desc;
    isa_device_desc->setActive(true);
    isa_device_desc->setDevice(dev);

    uint32_t size = isa_device_desc->getResource().portSize;
    uint32_t addr = isa_device_desc->getResource().portBase;
    uint32_t irq = isa_device_desc->getResource().irq;

    std::stringstream ss;
    ss << dev->qdev.id << "-io";

    memory_region_init_io(&symb_isa_state->io, &symbhw_io_ops, symb_isa_state, ss.str().c_str(), size);

    hw->setSymbolicPortRange(addr, size, true);
    isa_init_irq(dev, &symb_isa_state->qirq, irq);
    isa_device_desc->assignIrq(&symb_isa_state->qirq);

    return 0;
}


static void isa_symbhw_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    ISADeviceClass *k = ISA_DEVICE_CLASS(klass);

    IsaDeviceDescriptor *isa_desc = static_cast<IsaDeviceDescriptor*>(data);

    k->init = isa_symbhw_init;

    dc->vmsd = isa_desc->getVmStateDescription();
    dc->props = isa_desc->getProperties();
}

/////////////////////////////////////////////////////////////////////

static int pci_symbhw_init(PCIDevice *pci_dev)
{
    SymbolicPciDeviceState *symb_pci_state = DO_UPCAST(SymbolicPciDeviceState, dev, pci_dev);
    uint8_t *pci_conf;

    s2e_debug_print("pci_symbhw_init\n");

    //Retrive the configuration
    SymbolicHardware *hw = static_cast<SymbolicHardware*>(g_s2e->getPlugin("SymbolicHardware"));
    assert(hw);

    PciDeviceDescriptor *pci_device_desc = static_cast<PciDeviceDescriptor*>(hw->findDevice(pci_dev->name));
    assert(pci_device_desc);

    pci_device_desc->setActive(true);

    symb_pci_state->desc = pci_device_desc;
    pci_device_desc->setDevice(&symb_pci_state->dev);

    pci_conf = symb_pci_state->dev.config;
    pci_conf[PCI_HEADER_TYPE] = PCI_HEADER_TYPE_NORMAL; // header_type
    pci_conf[0x3d] = pci_device_desc->getInterruptPin(); // interrupt pin 0

    const PciDeviceDescriptor::PciResources &resources =
            pci_device_desc->getResources();

    unsigned i=0;
    foreach2(it, resources.begin(), resources.end()) {
        const PciDeviceDescriptor::PciResource &res = *it;
        int type = 0;

        type |= res.isIo ? PCI_BASE_ADDRESS_SPACE_IO : PCI_BASE_ADDRESS_SPACE_MEMORY;
        type |= res.prefetchable ? PCI_BASE_ADDRESS_MEM_PREFETCH : 0;

        std::stringstream ss;
        ss << pci_device_desc->getId();

        if (type & PCI_BASE_ADDRESS_SPACE_IO) {
            ss << "-io";
        } else if (type & PCI_BASE_ADDRESS_SPACE_MEMORY) {
            ss << "-mmio";
        }

        memory_region_init_io(&symb_pci_state->io[i], &symbhw_io_ops, symb_pci_state, ss.str().c_str(), res.size);
        pci_register_bar(&symb_pci_state->dev, i, type, &symb_pci_state->io[i]);
        ++i;
    }

    pci_device_desc->assignIrq(&symb_pci_state->dev.irq[0]);

    return 0;
}

static int pci_symbhw_uninit(PCIDevice *pci_dev)
{
    SymbolicPciDeviceState *d = DO_UPCAST(SymbolicPciDeviceState, dev, pci_dev);

    for (int i=0; i<d->desc->getResources().size(); ++i) {
        memory_region_destroy(&d->io[i]);
    }

    return 0;
}

static void  pci_symbhw_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    PCIDeviceClass *k = PCI_DEVICE_CLASS(klass);

    PciDeviceDescriptor *pci_desc = static_cast<PciDeviceDescriptor*>(data);

    k->init = pci_symbhw_init;
    k->exit = pci_symbhw_uninit;

    k->vendor_id = pci_desc->getVid();
    k->device_id = pci_desc->getPid();
    k->revision = pci_desc->getRevisionId();
    k->class_id = pci_desc->getClassCode();
    k->subsystem_vendor_id = pci_desc->getSsVid();
    k->subsystem_id = pci_desc->getSsId();

    dc->vmsd = pci_desc->getVmStateDescription();
    dc->props = pci_desc->getProperties();
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
        MemoryRanges::iterator it = m_MmioMemory.find(addr & TARGET_PAGE_MASK);
        if (it == m_MmioMemory.end()) {
            if (!b) {
                //No need to reset anything,
                //Go to the next page
                uint64_t leftover = TARGET_PAGE_SIZE - (addr & (TARGET_PAGE_SIZE-1));
                addr += leftover;
                size -= leftover > size ? size : leftover;
                continue;
            }else {
                //Need to create a new page
                m_MmioMemory[addr & TARGET_PAGE_MASK] = PageBitmap();
                it = m_MmioMemory.find(addr & TARGET_PAGE_MASK);
            }
        }

        uint32_t offset = addr & (TARGET_PAGE_SIZE-1);
        uint32_t mysize = offset + size > TARGET_PAGE_SIZE ? TARGET_PAGE_SIZE - offset : size;

        bool fc = (*it).second.set(offset, mysize, b);
        if (fc) {
            //The entire page is concrete, do not need to keep it in the map
            m_MmioMemory.erase(addr & TARGET_PAGE_MASK);
        }

        size -= mysize;
        addr += mysize;
    }
    return true;
}

bool SymbolicHardwareState::isMmio(uint64_t physaddr, uint64_t size) const
{
    while (size > 0) {
        MemoryRanges::const_iterator it = m_MmioMemory.find(physaddr & TARGET_PAGE_MASK);
        if (it == m_MmioMemory.end()) {
            uint64_t leftover = TARGET_PAGE_SIZE - (physaddr & (TARGET_PAGE_SIZE-1));
            physaddr += leftover;
            size -= leftover > size ? size : leftover;
            continue;
        }

        if (((physaddr & (TARGET_PAGE_SIZE-1)) == 0) && size>=TARGET_PAGE_SIZE) {
            if ((*it).second.hasSymbolic()) {
                return true;
            }
            size-=TARGET_PAGE_SIZE;
            physaddr+=TARGET_PAGE_SIZE;
            continue;
        }

        bool b = (*it).second.get(physaddr & (TARGET_PAGE_SIZE-1));
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
        bitmap = new klee::BitArray(*source, TARGET_PAGE_SIZE);
    } else {
        bitmap = new klee::BitArray(TARGET_PAGE_SIZE, false);
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
        return bitmap->isAllZeros(TARGET_PAGE_SIZE);
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
    assert(offset <= TARGET_PAGE_SIZE && offset + length <= TARGET_PAGE_SIZE);
    allocateBitmap(NULL);

    for (unsigned i=offset; i<offset + length; ++i) {
        bitmap->set(i, b);
    }

    fullySymbolic = bitmap->isAllOnes(TARGET_PAGE_SIZE);
    bool fc = false;
    if (fullySymbolic || (fc = bitmap->isAllZeros(TARGET_PAGE_SIZE))) {
        delete bitmap;
        bitmap = NULL;
        return fc;
    }
    return false;
}

bool SymbolicHardwareState::PageBitmap::get(unsigned offset) const {
    assert(offset < TARGET_PAGE_SIZE);
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
