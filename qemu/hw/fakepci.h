#ifndef FAKE_PCI_H

#define FAKE_PCI_H

#include "pci.h"


typedef struct _fake_pci_t {
    const char *name;
    int vendor_id;
    int device_id;
    int revision_id;
    int class_code;
    int ss_vendor_id;
    int ss_id;
    int num_resources;
    PCIIORegion resources[PCI_NUM_REGIONS];
    int mmioidx;
}fake_pci_t;

enum fake_bus_type_t {
    ISA, PCI
};

#ifndef CONFIG_S2E

extern fake_pci_t g_fake_pci;
void fakepci_register_device(fake_pci_t *fake);
void fakepci_activate_device(enum fake_bus_type_t type, void *bus);

#endif

#endif
