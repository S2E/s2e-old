/*
 * ARM Integrator CP System emulation.
 *
 * Copyright (c) 2005-2007 CodeSourcery.
 * Written by Paul Brook
 *
 * This code is licensed under the GPL
 */

#include "sysbus.h"
#include "devices.h"
#include "boards.h"
#include "arm-misc.h"
#include "net.h"
#include "exec-memory.h"
#include "sysemu.h"

#ifdef CONFIG_S2E
#include <s2e/s2e_qemu.h>
#endif

typedef struct {
    SysBusDevice busdev;
    MemoryRegion iomem;
    uint32_t memsz;
    MemoryRegion flash;
    uint32_t cm_osc;
    uint32_t cm_ctrl;
    uint32_t cm_lock;
    uint32_t cm_auxosc;
    uint32_t cm_sdram;
    uint32_t cm_init;
    uint32_t cm_flags;
    uint32_t cm_nvflags;
    uint32_t int_level;
    uint32_t irq_enabled;
    uint32_t fiq_enabled;
} integratorcm_state;

static uint8_t integrator_spd[128] = {
   128, 8, 4, 11, 9, 1, 64, 0,  2, 0xa0, 0xa0, 0, 0, 8, 0, 1,
   0xe, 4, 0x1c, 1, 2, 0x20, 0xc0, 0, 0, 0, 0, 0x30, 0x28, 0x30, 0x28, 0x40
};

static uint64_t integratorcm_read(void *opaque, target_phys_addr_t offset,
                                  unsigned size)
{
    integratorcm_state *s = (integratorcm_state *)opaque;
    if (offset >= 0x100 && offset < 0x200) {
        /* CM_SPD */
        if (offset >= 0x180)
            return 0;
        return integrator_spd[offset >> 2];
    }
    switch (offset >> 2) {
    case 0: /* CM_ID */
        return 0x411a3001;
    case 1: /* CM_PROC */
        return 0;
    case 2: /* CM_OSC */
        return s->cm_osc;
    case 3: /* CM_CTRL */
        return s->cm_ctrl;
    case 4: /* CM_STAT */
        return 0x00100000;
    case 5: /* CM_LOCK */
        if (s->cm_lock == 0xa05f) {
            return 0x1a05f;
        } else {
            return s->cm_lock;
        }
    case 6: /* CM_LMBUSCNT */
        /* ??? High frequency timer.  */
        hw_error("integratorcm_read: CM_LMBUSCNT");
    case 7: /* CM_AUXOSC */
        return s->cm_auxosc;
    case 8: /* CM_SDRAM */
        return s->cm_sdram;
    case 9: /* CM_INIT */
        return s->cm_init;
    case 10: /* CM_REFCT */
        /* ??? High frequency timer.  */
        hw_error("integratorcm_read: CM_REFCT");
    case 12: /* CM_FLAGS */
        return s->cm_flags;
    case 14: /* CM_NVFLAGS */
        return s->cm_nvflags;
    case 16: /* CM_IRQ_STAT */
        return s->int_level & s->irq_enabled;
    case 17: /* CM_IRQ_RSTAT */
        return s->int_level;
    case 18: /* CM_IRQ_ENSET */
        return s->irq_enabled;
    case 20: /* CM_SOFT_INTSET */
        return s->int_level & 1;
    case 24: /* CM_FIQ_STAT */
        return s->int_level & s->fiq_enabled;
    case 25: /* CM_FIQ_RSTAT */
        return s->int_level;
    case 26: /* CM_FIQ_ENSET */
        return s->fiq_enabled;
    case 32: /* CM_VOLTAGE_CTL0 */
    case 33: /* CM_VOLTAGE_CTL1 */
    case 34: /* CM_VOLTAGE_CTL2 */
    case 35: /* CM_VOLTAGE_CTL3 */
        /* ??? Voltage control unimplemented.  */
        return 0;
    default:
        hw_error("integratorcm_read: Unimplemented offset 0x%x\n",
                 (int)offset);
        return 0;
    }
}

static void integratorcm_do_remap(integratorcm_state *s)
{
    /* Sync memory region state with CM_CTRL REMAP bit:
     * bit 0 => flash at address 0; bit 1 => RAM
     */
    memory_region_set_enabled(&s->flash, !(s->cm_ctrl & 4));
}

static void integratorcm_set_ctrl(integratorcm_state *s, uint32_t value)
{
    if (value & 8) {
        qemu_system_reset_request();
    }
    if ((s->cm_ctrl ^ value) & 1) {
        /* (value & 1) != 0 means the green "MISC LED" is lit.
         * We don't have any nice place to display LEDs. printf is a bad
         * idea because Linux uses the LED as a heartbeat and the output
         * will swamp anything else on the terminal.
         */
    }
    /* Note that the RESET bit [3] always reads as zero */
    s->cm_ctrl = (s->cm_ctrl & ~5) | (value & 5);
    integratorcm_do_remap(s);
}

static void integratorcm_update(integratorcm_state *s)
{
    /* ??? The CPU irq/fiq is raised when either the core module or base PIC
       are active.  */
    if (s->int_level & (s->irq_enabled | s->fiq_enabled))
        hw_error("Core module interrupt\n");
}

static void integratorcm_write(void *opaque, target_phys_addr_t offset,
                               uint64_t value, unsigned size)
{
    integratorcm_state *s = (integratorcm_state *)opaque;
    switch (offset >> 2) {
    case 2: /* CM_OSC */
        if (s->cm_lock == 0xa05f)
            s->cm_osc = value;
        break;
    case 3: /* CM_CTRL */
        integratorcm_set_ctrl(s, value);
        break;
    case 5: /* CM_LOCK */
        s->cm_lock = value & 0xffff;
        break;
    case 7: /* CM_AUXOSC */
        if (s->cm_lock == 0xa05f)
            s->cm_auxosc = value;
        break;
    case 8: /* CM_SDRAM */
        s->cm_sdram = value;
        break;
    case 9: /* CM_INIT */
        /* ??? This can change the memory bus frequency.  */
        s->cm_init = value;
        break;
    case 12: /* CM_FLAGSS */
        s->cm_flags |= value;
        break;
    case 13: /* CM_FLAGSC */
        s->cm_flags &= ~value;
        break;
    case 14: /* CM_NVFLAGSS */
        s->cm_nvflags |= value;
        break;
    case 15: /* CM_NVFLAGSS */
        s->cm_nvflags &= ~value;
        break;
    case 18: /* CM_IRQ_ENSET */
        s->irq_enabled |= value;
        integratorcm_update(s);
        break;
    case 19: /* CM_IRQ_ENCLR */
        s->irq_enabled &= ~value;
        integratorcm_update(s);
        break;
    case 20: /* CM_SOFT_INTSET */
        s->int_level |= (value & 1);
        integratorcm_update(s);
        break;
    case 21: /* CM_SOFT_INTCLR */
        s->int_level &= ~(value & 1);
        integratorcm_update(s);
        break;
    case 26: /* CM_FIQ_ENSET */
        s->fiq_enabled |= value;
        integratorcm_update(s);
        break;
    case 27: /* CM_FIQ_ENCLR */
        s->fiq_enabled &= ~value;
        integratorcm_update(s);
        break;
    case 32: /* CM_VOLTAGE_CTL0 */
    case 33: /* CM_VOLTAGE_CTL1 */
    case 34: /* CM_VOLTAGE_CTL2 */
    case 35: /* CM_VOLTAGE_CTL3 */
        /* ??? Voltage control unimplemented.  */
        break;
    default:
        hw_error("integratorcm_write: Unimplemented offset 0x%x\n",
                 (int)offset);
        break;
    }
}

/* Integrator/CM control registers.  */

static const MemoryRegionOps integratorcm_ops = {
    .read = integratorcm_read,
    .write = integratorcm_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static int integratorcm_init(SysBusDevice *dev)
{
    integratorcm_state *s = FROM_SYSBUS(integratorcm_state, dev);

    s->cm_osc = 0x01000048;
    /* ??? What should the high bits of this value be?  */
    s->cm_auxosc = 0x0007feff;
    s->cm_sdram = 0x00011122;
    if (s->memsz >= 256) {
        integrator_spd[31] = 64;
        s->cm_sdram |= 0x10;
    } else if (s->memsz >= 128) {
        integrator_spd[31] = 32;
        s->cm_sdram |= 0x0c;
    } else if (s->memsz >= 64) {
        integrator_spd[31] = 16;
        s->cm_sdram |= 0x08;
    } else if (s->memsz >= 32) {
        integrator_spd[31] = 4;
        s->cm_sdram |= 0x04;
    } else {
        integrator_spd[31] = 2;
    }
    memcpy(integrator_spd + 73, "QEMU-MEMORY", 11);
    s->cm_init = 0x00000112;
    memory_region_init_ram(&s->flash, "integrator.flash", 0x100000);
    vmstate_register_ram_global(&s->flash);

    memory_region_init_io(&s->iomem, &integratorcm_ops, s,
                          "integratorcm", 0x00800000);
    sysbus_init_mmio(dev, &s->iomem);

    integratorcm_do_remap(s);
    /* ??? Save/restore.  */
    return 0;
}

/* Integrator/CP hardware emulation.  */
/* Primary interrupt controller.  */

typedef struct icp_pic_state
{
  SysBusDevice busdev;
  MemoryRegion iomem;
  uint32_t level;
  uint32_t irq_enabled;
  uint32_t fiq_enabled;
  qemu_irq parent_irq;
  qemu_irq parent_fiq;
} icp_pic_state;

static void icp_pic_update(icp_pic_state *s)
{
    uint32_t flags;

    flags = (s->level & s->irq_enabled);
    qemu_set_irq(s->parent_irq, flags != 0);
    flags = (s->level & s->fiq_enabled);
    qemu_set_irq(s->parent_fiq, flags != 0);
}

static void icp_pic_set_irq(void *opaque, int irq, int level)
{
    icp_pic_state *s = (icp_pic_state *)opaque;
    if (level)
        s->level |= 1 << irq;
    else
        s->level &= ~(1 << irq);
    icp_pic_update(s);
}

static uint64_t icp_pic_read(void *opaque, target_phys_addr_t offset,
                             unsigned size)
{
    icp_pic_state *s = (icp_pic_state *)opaque;

    switch (offset >> 2) {
    case 0: /* IRQ_STATUS */
        return s->level & s->irq_enabled;
    case 1: /* IRQ_RAWSTAT */
        return s->level;
    case 2: /* IRQ_ENABLESET */
        return s->irq_enabled;
    case 4: /* INT_SOFTSET */
        return s->level & 1;
    case 8: /* FRQ_STATUS */
        return s->level & s->fiq_enabled;
    case 9: /* FRQ_RAWSTAT */
        return s->level;
    case 10: /* FRQ_ENABLESET */
        return s->fiq_enabled;
    case 3: /* IRQ_ENABLECLR */
    case 5: /* INT_SOFTCLR */
    case 11: /* FRQ_ENABLECLR */
    default:
        printf ("icp_pic_read: Bad register offset 0x%x\n", (int)offset);
        return 0;
    }
}

static void icp_pic_write(void *opaque, target_phys_addr_t offset,
                          uint64_t value, unsigned size)
{
    icp_pic_state *s = (icp_pic_state *)opaque;

    switch (offset >> 2) {
    case 2: /* IRQ_ENABLESET */
        s->irq_enabled |= value;
        break;
    case 3: /* IRQ_ENABLECLR */
        s->irq_enabled &= ~value;
        break;
    case 4: /* INT_SOFTSET */
        if (value & 1)
            icp_pic_set_irq(s, 0, 1);
        break;
    case 5: /* INT_SOFTCLR */
        if (value & 1)
            icp_pic_set_irq(s, 0, 0);
        break;
    case 10: /* FRQ_ENABLESET */
        s->fiq_enabled |= value;
        break;
    case 11: /* FRQ_ENABLECLR */
        s->fiq_enabled &= ~value;
        break;
    case 0: /* IRQ_STATUS */
    case 1: /* IRQ_RAWSTAT */
    case 8: /* FRQ_STATUS */
    case 9: /* FRQ_RAWSTAT */
    default:
        printf ("icp_pic_write: Bad register offset 0x%x\n", (int)offset);
        return;
    }
    icp_pic_update(s);
}

static const MemoryRegionOps icp_pic_ops = {
    .read = icp_pic_read,
    .write = icp_pic_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static int icp_pic_init(SysBusDevice *dev)
{
    icp_pic_state *s = FROM_SYSBUS(icp_pic_state, dev);

    qdev_init_gpio_in(&dev->qdev, icp_pic_set_irq, 32);
    sysbus_init_irq(dev, &s->parent_irq);
    sysbus_init_irq(dev, &s->parent_fiq);
    memory_region_init_io(&s->iomem, &icp_pic_ops, s, "icp-pic", 0x00800000);
    sysbus_init_mmio(dev, &s->iomem);
    return 0;
}

/* CP control registers.  */

static uint64_t icp_control_read(void *opaque, target_phys_addr_t offset,
                                 unsigned size)
{
    switch (offset >> 2) {
    case 0: /* CP_IDFIELD */
        return 0x41034003;
    case 1: /* CP_FLASHPROG */
        return 0;
    case 2: /* CP_INTREG */
        return 0;
    case 3: /* CP_DECODE */
        return 0x11;
    default:
        hw_error("icp_control_read: Bad offset %x\n", (int)offset);
        return 0;
    }
}

static void icp_control_write(void *opaque, target_phys_addr_t offset,
                          uint64_t value, unsigned size)
{
    switch (offset >> 2) {
    case 1: /* CP_FLASHPROG */
    case 2: /* CP_INTREG */
    case 3: /* CP_DECODE */
        /* Nothing interesting implemented yet.  */
        break;
    default:
        hw_error("icp_control_write: Bad offset %x\n", (int)offset);
    }
}

static const MemoryRegionOps icp_control_ops = {
    .read = icp_control_read,
    .write = icp_control_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static void icp_control_init(target_phys_addr_t base)
{
    MemoryRegion *io;

    io = (MemoryRegion *)g_malloc0(sizeof(MemoryRegion));
    memory_region_init_io(io, &icp_control_ops, NULL,
                          "control", 0x00800000);
    memory_region_add_subregion(get_system_memory(), base, io);
    /* ??? Save/restore.  */
}


/* Board init.  */

static struct arm_boot_info integrator_binfo = {
    .loader_start = 0x0,
    .board_id = 0x113,
};

static void integratorcp_init(ram_addr_t ram_size,
                     const char *boot_device,
                     const char *kernel_filename, const char *kernel_cmdline,
                     const char *initrd_filename, const char *cpu_model)
{
    CPUARMState *env;
    MemoryRegion *address_space_mem = get_system_memory();
    MemoryRegion *ram = g_new(MemoryRegion, 1);
    MemoryRegion *ram_alias = g_new(MemoryRegion, 1);
    qemu_irq pic[32];
    qemu_irq *cpu_pic;
    DeviceState *dev;
    int i;

    if (!cpu_model)
        cpu_model = "arm926";
    env = cpu_init(cpu_model);
    if (!env) {
        fprintf(stderr, "Unable to find CPU definition\n");
        exit(1);
    }
    memory_region_init_ram(ram, "integrator.ram", ram_size);
    vmstate_register_ram_global(ram);

#ifdef CONFIG_S2E
    s2e_register_cpu(g_s2e, g_s2e_state, env);
#endif

    /* ??? On a real system the first 1Mb is mapped as SSRAM or boot flash.  */
    /* ??? RAM should repeat to fill physical memory space.  */
    /* SDRAM at address zero*/
    memory_region_add_subregion(address_space_mem, 0, ram);
    /* And again at address 0x80000000 */
    memory_region_init_alias(ram_alias, "ram.alias", ram, 0, ram_size);
    memory_region_add_subregion(address_space_mem, 0x80000000, ram_alias);

#ifdef CONFIG_S2E
    s2e_register_ram(g_s2e, g_s2e_state,
                  0, ram_size,
                  (uint64_t) memory_region_get_ram_ptr(ram), 0, 0, "ram");
//    s2e_register_ram(g_s2e, g_s2e_state,
//    			  0x80000000, ram_size,
//    			  (uint64_t) qemu_get_ram_ptr(ram_offset | IO_MEM_RAM), 0,0, "ramhigh");
#endif

    dev = qdev_create(NULL, "integrator_core");
    qdev_prop_set_uint32(dev, "memsz", ram_size >> 20);
    qdev_init_nofail(dev);
    sysbus_mmio_map((SysBusDevice *)dev, 0, 0x10000000);

    cpu_pic = arm_pic_init_cpu(env);
    dev = sysbus_create_varargs("integrator_pic", 0x14000000,
                                cpu_pic[ARM_PIC_CPU_IRQ],
                                cpu_pic[ARM_PIC_CPU_FIQ], NULL);
    for (i = 0; i < 32; i++) {
        pic[i] = qdev_get_gpio_in(dev, i);
    }
    sysbus_create_simple("integrator_pic", 0xca000000, pic[26]);
    sysbus_create_varargs("integrator_pit", 0x13000000,
                          pic[5], pic[6], pic[7], NULL);
    sysbus_create_simple("pl031", 0x15000000, pic[8]);
    sysbus_create_simple("pl011", 0x16000000, pic[1]);
    sysbus_create_simple("pl011", 0x17000000, pic[2]);
    icp_control_init(0xcb000000);
    sysbus_create_simple("pl050_keyboard", 0x18000000, pic[3]);
    sysbus_create_simple("pl050_mouse", 0x19000000, pic[4]);
    sysbus_create_varargs("pl181", 0x1c000000, pic[23], pic[24], NULL);
    if (nd_table[0].vlan)
        smc91c111_init(&nd_table[0], 0xc8000000, pic[27]);

    sysbus_create_simple("pl110", 0xc0000000, pic[22]);

    integrator_binfo.ram_size = ram_size;
    integrator_binfo.kernel_filename = kernel_filename;
    integrator_binfo.kernel_cmdline = kernel_cmdline;
    integrator_binfo.initrd_filename = initrd_filename;
    arm_load_kernel(env, &integrator_binfo);
}

static QEMUMachine integratorcp_machine = {
    .name = "integratorcp",
    .desc = "ARM Integrator/CP (ARM926EJ-S)",
    .init = integratorcp_init,
    .is_default = 1,
};

static void integratorcp_machine_init(void)
{
    qemu_register_machine(&integratorcp_machine);
}

machine_init(integratorcp_machine_init);

static Property core_properties[] = {
    DEFINE_PROP_UINT32("memsz", integratorcm_state, memsz, 0),
    DEFINE_PROP_END_OF_LIST(),
};

static void core_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    SysBusDeviceClass *k = SYS_BUS_DEVICE_CLASS(klass);

    k->init = integratorcm_init;
    dc->props = core_properties;
}

static TypeInfo core_info = {
    .name          = "integrator_core",
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(integratorcm_state),
    .class_init    = core_class_init,
};

static void icp_pic_class_init(ObjectClass *klass, void *data)
{
    SysBusDeviceClass *sdc = SYS_BUS_DEVICE_CLASS(klass);

    sdc->init = icp_pic_init;
}

static TypeInfo icp_pic_info = {
    .name          = "integrator_pic",
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(icp_pic_state),
    .class_init    = icp_pic_class_init,
};

static void integratorcp_register_types(void)
{
    type_register_static(&icp_pic_info);
    type_register_static(&core_info);
}

type_init(integratorcp_register_types)
