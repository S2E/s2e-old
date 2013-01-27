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
 *    Volodymyr Kuznetsov <vova.kuznetsov@epfl.ch>
 *    Vitaly Chipounov <vitaly.chipounov@epfl.ch>
 *
 * All contributors are listed in the S2E-AUTHORS file.
 */

/**
 *  This module enables fake pci device support for native QEMU.
 *  This allows quick creation of snapshots in native mode, ready to be run in S2E.
 */


#include <stdio.h>
#include "qemu-common.h"
#include "hw/hw.h"
#include "hw/pci.h"
#include "hw/msi.h" // MSI support
#include "hw/pcie.h" // PCI-E support

#include "fakepci.h"


static fake_pci_t *s_fake_pci=NULL;
PCIDevice *fake_pci_dev;


/////////////////////////////////////////////////////////////////////
/* Dummy I/O functions for symbolic devices. Unused for now. */
static uint64_t fake_read(void *opaque, target_phys_addr_t addr,
                            unsigned size)
{
    return 0;
}

static void fake_write(void *opaque, target_phys_addr_t addr,
                         uint64_t data, unsigned size)
{

}

static const MemoryRegionOps fake_ops = {
    .read = fake_read,
    .write = fake_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};


typedef struct _PCIFakeState {
    PCIDevice dev;
    fake_pci_t fake_pci;
    MemoryRegion io[PCI_NUM_REGIONS];
} PCIFakeState;

static int pci_fake_init(PCIDevice *pci_dev)
{
    PCIFakeState *d = DO_UPCAST(PCIFakeState, dev, pci_dev);
    uint8_t *pci_conf;
    int i;

    d->fake_pci = *s_fake_pci;

    pci_conf = d->dev.config;
    pci_conf[PCI_HEADER_TYPE] = PCI_HEADER_TYPE_NORMAL; // header_type
    pci_conf[PCI_INTERRUPT_PIN] = 1; // interrupt pin 0

    char *name_io = malloc(strlen(d->fake_pci.name) + 20);
    char *name_mmio = malloc(strlen(d->fake_pci.name) + 20);
    sprintf(name_io, "%s-io", d->fake_pci.name);
    sprintf(name_mmio, "%s-mmio", d->fake_pci.name);

    if (d->fake_pci.cap_pm > 0) {
        // Force PCI power management to ON
        int r = pci_add_capability(pci_dev, PCI_CAP_ID_PM, 0, PCI_PM_SIZEOF);
        assert (r >= 0 && "Why isn't power management working?");
    }

    if (d->fake_pci.cap_msi > 0) {
        // The 0 = find a valid PCI capability offset.
        // 0x50 seems to work FWIW
        // The first 64 bytes of PCI config space are
        // standardized, so 0x50 = the first byte after that.
        // If we add more capabilities this number might need
        // to be changed.
        // false = msi64bit (4th param)
        // false = msi_per_vector_mask (5th param)
        msi_init(pci_dev, 0, d->fake_pci.cap_msi, false, false);
    } else {
        assert (d->fake_pci.cap_msi == 0 && "?? MSI should be >= 0");
    }

    if (d->fake_pci.cap_pcie > 0) {
        int r = pcie_cap_init(pci_dev, 0, PCI_EXP_TYPE_ENDPOINT, 0);
        assert (r >= 0 && "Why isn't PCI-E working?");
    }

    if (d->fake_pci.cap_pm > 0) {
        assert (pci_find_capability(pci_dev, PCI_CAP_ID_PM) != 0 && "cap PM bug.");
    }
    if (d->fake_pci.cap_msi > 0) {
        assert (pci_find_capability(pci_dev, PCI_CAP_ID_MSI) != 0 && "cap MSI bug.");
    }
    if (d->fake_pci.cap_pcie > 0) {
        assert (pci_find_capability(pci_dev, PCI_CAP_ID_EXP) != 0 && "cap PCI-E bug.");
    }

    for(i=0; i<d->fake_pci.num_resources; ++i) {
        int type = d->fake_pci.resources[i].type;
        int size = d->fake_pci.resources[i].size;
        const char *name = "fakepci-io";

        if (type == PCI_BASE_ADDRESS_SPACE_IO) {
            // Port I/O
            name = name_io;
        } else {
            // Memory-mapped I/O
            // No need to explicitly check PCI_BASE_ADDRESS_SPACE_MEMORY
            // This preprocessor directive evaluates to zero and isn't a flag
            name = name_mmio;
        }

        memory_region_init_io(&d->io[i], &fake_ops, d, name, size);
        pci_register_bar(&d->dev, i, type, &d->io[i]); //, pci_fake_map
    }

    free(name_io);
    free(name_mmio);

    return 0;
}

static int pci_fake_uninit(PCIDevice *dev)
{
    PCIFakeState *d = DO_UPCAST(PCIFakeState, dev, dev);

    for (int i=0; i<d->fake_pci.num_resources; ++i) {
        memory_region_destroy(&d->io[i]);
    }

    // PM support requires no special shutdown

    // MSI support
    if (d->fake_pci.cap_msi > 0) {
        msi_uninit(dev);
    }

    // PCI-E support
    if (d->fake_pci.cap_pcie > 0) {
        pcie_cap_exit(dev);
    }

    return 0;
}

static VMStateDescription vmstate_pci_fake = {
    .name = "fakepci",
    .version_id = 3,
    .minimum_version_id = 3,
    .minimum_version_id_old = 3,
    .fields      = (VMStateField []) {
        VMSTATE_PCI_DEVICE(dev, PCIFakeState),
        VMSTATE_END_OF_LIST()
    }
};

static VMStateDescription vmstate_pcie_fake = {
    .name = "fakepci",
    .version_id = 3,
    .minimum_version_id = 3,
    .minimum_version_id_old = 3,
    .fields      = (VMStateField []) {
        VMSTATE_PCIE_DEVICE(dev, PCIFakeState),
        VMSTATE_END_OF_LIST()
    }
};

static Property fakepci_properties[] = {
    DEFINE_PROP_END_OF_LIST(),
};

static void fakepci_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    PCIDeviceClass *k = PCI_DEVICE_CLASS(klass);

    // PCI-E support
    if (s_fake_pci->cap_pcie > 0) {
        k->is_express = 1;
    }

    k->init = pci_fake_init;
    k->exit = pci_fake_uninit;

    k->vendor_id = s_fake_pci->vendor_id;
    k->device_id = s_fake_pci->device_id;
    k->revision = s_fake_pci->revision_id;
    k->class_id = s_fake_pci->class_code;
    k->subsystem_vendor_id = s_fake_pci->ss_vendor_id;
    k->subsystem_id = s_fake_pci->ss_id;

    if (s_fake_pci->cap_pcie > 0) {
        dc->vmsd = &vmstate_pcie_fake;
    } else {
        dc->vmsd = &vmstate_pci_fake;
    }

    dc->props = fakepci_properties;
}

static TypeInfo fakepci_info = {
    /* The name is changed at registration time */
    .name          = "fakepci",

    .parent        = TYPE_PCI_DEVICE,
    .instance_size = sizeof(PCIFakeState),
    .class_init    = fakepci_class_init,
};

/**
 * Both vanilla QEMU and S2E must register the devices at the exact
 * same point during VM startup, in order to ensure that the device is
 * connected the same in the PCI subsystem (same slots, etc.).
 * Failing to do so may prevent the VM snapshots from resuming.
 */
void fakepci_register_device(fake_pci_t *fake)
{
    /* Check whether the user enabled a PCI device */
    if (!fake->name) {
        return;
    }

    /* Clone the passed structure */
    s_fake_pci = malloc(sizeof(fake_pci_t));
    if (!s_fake_pci) {
        perror("Could not allocate fake pci device\n");
    }

    *s_fake_pci = *fake;
    s_fake_pci->name = strdup(fake->name);

    if (!s_fake_pci->name) {
        perror("Could not allocate fake pci device name string\n");
    }


    fakepci_info.name = s_fake_pci->name;

    // Just assign both -- we'll only actually use one of these though
    vmstate_pcie_fake.name = s_fake_pci->name;
    vmstate_pci_fake.name = s_fake_pci->name;

    type_register_static(&fakepci_info);
}


void fakepci_activate_device(enum fake_bus_type_t type, void *bus)
{
    if (!s_fake_pci) {
        return;
    }

    assert(type == PCI);
    pci_create_simple((struct PCIBus*) bus, -1, s_fake_pci->name);
}

