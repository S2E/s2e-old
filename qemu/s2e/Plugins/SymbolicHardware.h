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

#ifndef S2E_PLUGINS_SYMBHW_H
#define S2E_PLUGINS_SYMBHW_H

#include <s2e/Plugin.h>
#include <s2e/Plugins/CorePlugin.h>
#include <s2e/S2EExecutionState.h>
#include <s2e/ConfigFile.h>

#include "klee/util/BitArray.h"

#include <string>
#include <set>
#include <map>

namespace s2e {
namespace plugins {

class SymbolicHardware;

class DeviceDescriptor {
protected:
    std::string m_id;
    void *m_qemuIrq;
    void *m_qemuDev;
    bool m_active;

    struct TypeInfo *m_devInfo;
    struct Property *m_devInfoProperties;
    struct VMStateDescription *m_vmState;
    struct _VMStateField *m_vmStateFields;

public:
    DeviceDescriptor(const std::string &id);

    static DeviceDescriptor *create(SymbolicHardware *plg, ConfigFile *cfg, const std::string &key);
    virtual ~DeviceDescriptor();

    struct comparator {
    bool operator()(const DeviceDescriptor *dd1, const DeviceDescriptor *dd2) const {
        return dd1->m_id < dd2->m_id;
    }
    };

    bool isActive() const {
        return m_active;
    }

    void setActive(bool b) {
        m_active = true;
    }

    void setDevice(void *qemuDev) {
        m_qemuDev = qemuDev;
    }

    const std::string &getId() const { return m_id; }

    virtual void print(llvm::raw_ostream &os) const {}
    virtual void initializeQemuDevice() {assert(false);}
    virtual void activateQemuDevice(void *bus) { assert(false);}
    virtual void setInterrupt(bool state) {assert(false);};
    virtual void assignIrq(void *irq) {assert(false);}
    virtual bool readPciAddressSpace(void *buffer, uint32_t offset, uint32_t size) {
        return false;
    }

    struct VMStateDescription* getVmStateDescription() const { return m_vmState; }
    struct Property* getProperties() const { return m_devInfoProperties; }

    virtual bool isPci() const { return false; }
    virtual bool isIsa() const { return false; }
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

    struct TypeInfo *m_isaInfo;
    struct Property *m_isaProperties;
    struct VMStateDescription *m_vmState;
    struct _VMStateField *m_vmStateFields;

public:
    IsaDeviceDescriptor(const std::string &id, const IsaResource &res);

    static IsaDeviceDescriptor* create(SymbolicHardware *plg, ConfigFile *cfg, const std::string &key);
    virtual ~IsaDeviceDescriptor();
    virtual void print(llvm::raw_ostream &os) const;
    virtual void initializeQemuDevice();
    virtual void activateQemuDevice(void *bus);

    const IsaResource& getResource() const {
        return m_isaResource;
    }

    virtual void setInterrupt(bool state);
    virtual void assignIrq(void *irq);

    virtual bool isPci() const { return false; }
    virtual bool isIsa() const { return true; }
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
    uint16_t m_ss_id;
    uint16_t m_ss_vid;
    uint32_t m_classCode;
    uint8_t m_revisionId;
    uint8_t m_interruptPin;
    PciResources m_resources;

    PciDeviceDescriptor(const std::string &id);
    virtual void print(llvm::raw_ostream &os) const;

public:
    virtual ~PciDeviceDescriptor();

    uint16_t getVid() const { return m_vid; }
    uint16_t getPid() const { return m_pid; }
    uint16_t getSsVid() const { return m_ss_vid; }
    uint16_t getSsId() const { return m_ss_id; }
    uint32_t getClassCode() const { return m_classCode; }
    uint8_t getRevisionId() const { return m_revisionId; }
    uint8_t getInterruptPin() const { return m_interruptPin; }

    const PciResources& getResources() const { return m_resources; }
    static PciDeviceDescriptor* create(SymbolicHardware *plg, ConfigFile *cfg, const std::string &key);

    virtual void initializeQemuDevice();
    virtual void activateQemuDevice(void *bus);

    virtual void setInterrupt(bool state);
    virtual void assignIrq(void *irq);

    virtual bool readPciAddressSpace(void *buffer, uint32_t offset, uint32_t size);



    virtual bool isPci() const { return true; }
    virtual bool isIsa() const { return false; }
};

class SymbolicHardware : public Plugin
{
    S2E_PLUGIN
public:

            typedef std::set<DeviceDescriptor *,DeviceDescriptor::comparator > DeviceDescriptors;


public:
    SymbolicHardware(S2E* s2e): Plugin(s2e) {}
    virtual ~SymbolicHardware();
    void initialize();

    DeviceDescriptor *findDevice(const std::string &name) const;

    void setSymbolicPortRange(uint16_t start, unsigned size, bool isSymbolic);
    bool isSymbolic(uint16_t port) const;

    bool isMmioSymbolic(uint64_t physaddress, uint64_t size) const;
    bool setSymbolicMmioRange(S2EExecutionState *state, uint64_t physaddr, uint64_t size);
    bool resetSymbolicMmioRange(S2EExecutionState *state, uint64_t physaddr, uint64_t size);
private:
    uint32_t m_portMap[65536/(sizeof(uint32_t)*8)];
    DeviceDescriptors m_devices;

    void onDeviceRegistration();
    void onDeviceActivation(int bus_type, void *bus);

};

class SymbolicHardwareState : public PluginState
{
public:
    class PageBitmap {
    private:
        bool fullySymbolic; //if false then bitmap must not be null
        klee::BitArray *bitmap; //bit set means symbolic

        void allocateBitmap(klee::BitArray *source);
        bool isFullySymbolic() const;
        bool isFullyConcrete() const;

    public:
        PageBitmap();
        PageBitmap(const PageBitmap &b1);
        ~PageBitmap();

        bool set(unsigned offset, unsigned length, bool b);
        bool get(unsigned offset) const;
        bool hasSymbolic() const;
    };

    typedef std::map<uint64_t, PageBitmap> MemoryRanges;
private:

    MemoryRanges m_MmioMemory;

public:

    SymbolicHardwareState();
    virtual ~SymbolicHardwareState();
    virtual SymbolicHardwareState* clone() const;
    static PluginState *factory(Plugin *p, S2EExecutionState *s);

    bool setMmioRange(uint64_t physbase, uint64_t size, bool b);
    bool isMmio(uint64_t physaddr, uint64_t size) const;

    friend class SymbolicHardware;

};

} // namespace plugins
} // namespace s2e

#endif // S2E_PLUGINS_EXAMPLE_H
