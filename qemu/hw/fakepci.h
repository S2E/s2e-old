#ifndef FAKE_PCI_H

#define FAKE_PCI_H

#include "pci.h"

#ifndef CONFIG_S2E
typedef struct {
    const char *fake_pci_name;
    int fake_pci_vendor_id;
    int fake_pci_device_id;
    int fake_pci_revision_id;
    int fake_pci_class_code;
    int fake_pci_ss_vendor_id;
    int fake_pci_ss_id;
    int fake_pci_num_resources;
    PCIIORegion fake_pci_resources[PCI_NUM_REGIONS];
    int mmioidx;
}fake_pci_t;

extern fake_pci_t g_fake_pci;

#endif

#endif
