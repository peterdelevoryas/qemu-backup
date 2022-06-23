/*
 * Copyright (c) Meta Platforms, Inc. and affiliates. (http://www.meta.com)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "qemu/osdep.h"
#include "qemu/units.h"
#include "qapi/error.h"
#include "sysemu/reset.h"
#include "hw/boards.h"
#include "hw/core/cpu.h"
#include "hw/qdev-clock.h"
#include "hw/qdev-properties.h"
#include "hw/arm/boot.h"
#include "hw/arm/aspeed_soc.h"
#include "hw/i2c/i2c.h"

#define FBY35_BMC_NR_CPUS (2)
#define FBY35_BMC_RAM_SIZE (2 * GiB)
#define FBY35_BMC_HW_STRAP1 (0x000000C0)
#define FBY35_BMC_HW_STRAP2 (0x00000003)

#define FBY35_BIC_NR_CPUS 1

#define FBY35_MACHINE_NR_CPUS (FBY35_BMC_NR_CPUS + FBY35_BIC_NR_CPUS)
#define FBY35_MACHINE_RAM_SIZE (FBY35_BMC_RAM_SIZE)

#define TYPE_FBY35_MACHINE MACHINE_TYPE_NAME("fby35")
OBJECT_DECLARE_SIMPLE_TYPE(Fby35MachineState, FBY35_MACHINE);

#define TYPE_FBY35_SYSTEM_BUS "fby35-system-bus"
OBJECT_DECLARE_SIMPLE_TYPE(Fby35SystemBus, FBY35_SYSTEM_BUS);

struct Fby35SystemBus {
    SysBusDevice parent_obj;
};

struct Fby35MachineState {
    MachineState parent_obj;

    MemoryRegion bmc_system_memory;
    MemoryRegion bmc_dram;
    MemoryRegion bmc_boot_rom;
    MemoryRegion bic_system_memory;
    MemoryRegion bic_boot_rom;
    Clock *bic_sysclk;
    I2CBus *slot0_i2c_bus;
    Fby35SystemBus system_bus;

    AspeedSoCState bmc;
    AspeedSoCState bic;
};

static uint8_t bmc_firmware[128 * MiB];

static void bmc_cpu_reset(void *opaque)
{
    CPUState *cpu = opaque;

    cpu_reset(cpu);
    cpu_set_pc(cpu, 0x00000000);
    dma_memory_write(cpu->as, 0, bmc_firmware, sizeof(bmc_firmware), MEMTXATTRS_UNSPECIFIED);
}

static void pull_up(void *opaque, int n, int level)
{
    printf("PULL UP\n");

    Object *obj = opaque;
    object_property_set_bool(obj, "gpioV4", true, &error_abort);
    object_property_set_bool(obj, "gpioV5", true, &error_abort);
    object_property_set_bool(obj, "gpioV6", true, &error_abort);
    object_property_set_bool(obj, "gpioV7", false, &error_abort);

    object_property_set_bool(obj, "gpioB2", true, &error_abort);
    object_property_set_bool(obj, "gpioB3", true, &error_abort);
    object_property_set_bool(obj, "gpioB4", true, &error_abort);
    object_property_set_bool(obj, "gpioB5", true, &error_abort);
}

static void fby35_bmc_init(MachineState *machine)
{
    Fby35MachineState *s = FBY35_MACHINE(machine);
    AspeedSoCClass *sc = NULL;

    memory_region_init(&s->bmc_system_memory, OBJECT(s), "bmc-system-memory", UINT64_MAX);
    memory_region_init(&s->bmc_dram, OBJECT(s), "bmc-dram", FBY35_BMC_RAM_SIZE);
    //memory_region_add_subregion(get_system_memory(), 0, &s->bmc_system_memory);
    memory_region_add_subregion(&s->bmc_dram, 0, machine->ram);

    object_initialize_child(OBJECT(s), "bmc", &s->bmc, "ast2600-a3");
    object_property_set_int(OBJECT(&s->bmc), "ram-size", FBY35_BMC_RAM_SIZE, &error_abort);
    object_property_set_link(OBJECT(&s->bmc), "system-memory", OBJECT(&s->bmc_system_memory), &error_abort);
    object_property_set_link(OBJECT(&s->bmc), "dram", OBJECT(&s->bmc_dram), &error_abort);
    object_property_set_link(OBJECT(&s->bmc), "i2c-bus0", OBJECT(s->slot0_i2c_bus), &error_abort);
    //object_property_set_bool(OBJECT(&s->bmc.cpu[0]), "start-powered-off", true, &error_abort);
    qdev_prop_set_uint32(DEVICE(&s->bmc), "hw-strap1", FBY35_BMC_HW_STRAP1);
    qdev_prop_set_uint32(DEVICE(&s->bmc), "hw-strap2", FBY35_BMC_HW_STRAP2);
    qdev_prop_set_uint32(DEVICE(&s->bmc), "uart-default", ASPEED_DEV_UART5);
    qdev_realize(DEVICE(&s->bmc), NULL, &error_abort);
    sc = ASPEED_SOC_GET_CLASS(&s->bmc);

    memory_region_add_subregion(&s->bmc_system_memory, sc->memmap[ASPEED_DEV_SDRAM], &s->bmc_dram);
    memory_region_init_rom(&s->bmc_boot_rom, OBJECT(s), "bmc-boot-rom", 128 * MiB, &error_abort);
    memory_region_add_subregion(&s->bmc_system_memory, 0, &s->bmc_boot_rom);

    aspeed_board_init_flashes(&s->bmc.fmc, "n25q00", 2, 0);

    for (int i = 4; i < 8; i++) {
        I2CBus *i2c = aspeed_i2c_get_bus(&s->bmc.i2c, i);
        i2c_slave_create_simple(i2c, "fby35-cpld", 0xf);
    }

    (void)bmc_cpu_reset;
    {
        printf("Reading fby35.mtd...");
        fflush(stdout);
        FILE *f = fopen("fby35.mtd", "r");
        assert(fread(bmc_firmware, sizeof(bmc_firmware), 1, f) == 1);
        fclose(f);
        printf("done\n");
    }

    address_space_write_rom(CPU(&s->bmc.cpu[0])->as, 0, MEMTXATTRS_UNSPECIFIED, bmc_firmware, sizeof(bmc_firmware));

    // dma_memory_write(CPU(&s->bmc.cpu[0])->as, 0, bmc_firmware, sizeof(bmc_firmware), MEMTXATTRS_UNSPECIFIED);
    // qemu_register_reset(bmc_cpu_reset, CPU(&s->bmc.cpu[0]));
}

static void fby35_bic_init(MachineState *machine)
{
    Fby35MachineState *s = FBY35_MACHINE(machine);

    s->bic_sysclk = clock_new(OBJECT(s), "SYSCLK");
    clock_set_hz(s->bic_sysclk, 200000000ULL);

    memory_region_init(&s->bic_system_memory, OBJECT(s), "bic-system-memory", UINT64_MAX);

    object_initialize_child(OBJECT(s), "bic", &s->bic, "ast1030-a1");
    qdev_connect_clock_in(DEVICE(&s->bic), "sysclk", s->bic_sysclk);
    object_property_set_link(OBJECT(&s->bic), "system-memory", OBJECT(&s->bic_system_memory), &error_abort);
    object_property_set_link(OBJECT(&s->bic), "i2c-bus2", OBJECT(s->slot0_i2c_bus), &error_abort);
    qdev_prop_set_uint32(DEVICE(&s->bic), "uart-default", ASPEED_DEV_UART5);
    //object_property_set_bool(OBJECT(&s->bic.armv7m), "start-powered-off", true, &error_abort);
    qdev_realize(DEVICE(&s->bic), NULL, &error_abort);

    // memory_region_init_rom(&s->bic_boot_rom, OBJECT(s), "bic-boot-rom", 128 * MiB, &error_abort);
    // memory_region_add_subregion(&s->bic_system_memory, 0, &s->bic_boot_rom);

    aspeed_board_init_flashes(&s->bic.fmc, "sst25vf032b", 2, 2);
    aspeed_board_init_flashes(&s->bic.spi[0], "sst25vf032b", 2, 4);
    aspeed_board_init_flashes(&s->bic.spi[1], "sst25vf032b", 2, 6);

    armv7m_load_kernel(s->bic.armv7m.cpu, "Y35BCL.elf", 1 * MiB);
    //armv7m_load_kernel(s->bic.armv7m.cpu, NULL, 1 * MiB);

    I2CBus *i2c[14];
    for (int i = 0; i < 14; i++) {
        i2c[i] = aspeed_i2c_get_bus(&s->bic.i2c, i);
    }
    aspeed_eeprom_init(i2c[1], 0x71, 64 * KiB);
    aspeed_eeprom_init(i2c[7], 0x20, 64 * KiB);
    aspeed_eeprom_init(i2c[8], 0x20, 64 * KiB);
    i2c_slave_create_simple(i2c[2], "intel-me", 0x16);
}

static void fby35_machine_init(MachineState *machine)
{
    Fby35MachineState *s = FBY35_MACHINE(machine);

    (void)fby35_bmc_init;
    (void)fby35_bic_init;

    object_initialize_child(OBJECT(s), "system-bus", &s->system_bus, TYPE_FBY35_SYSTEM_BUS);
    sysbus_realize(SYS_BUS_DEVICE(&s->system_bus), &error_abort);
    s->slot0_i2c_bus = i2c_init_bus(DEVICE(&s->system_bus), "slot0_i2c_bus");

    // aspeed_eeprom_init(s->slot0_i2c_bus, 0x16, 64 * KiB);

    fby35_bmc_init(machine);
    fby35_bic_init(machine);

    pull_up(&s->bmc.gpio, 0, false);
    qdev_init_gpio_in_named(DEVICE(&s->bmc.gpio), pull_up, "pull-up", 1);
    qdev_connect_gpio_out_named(DEVICE(&s->bmc.gpio), "sysbus-irq", 173,
                                qdev_get_gpio_in_named(DEVICE(&s->bmc.gpio), "pull-up", 0));

    BusChild *kid;
    QTAILQ_FOREACH(kid, &s->slot0_i2c_bus->qbus.children, sibling) {
        I2CSlave *dev = I2C_SLAVE(kid->child);
        printf("slot0 bus has slave 0x%02x\n", dev->address);
    }
}

static void fby35_machine_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    mc->desc = "Meta Platforms fby35";
    mc->init = fby35_machine_init;
    mc->no_floppy = 1;
    mc->no_cdrom = 1;
    mc->no_parallel = 1;
    mc->default_ram_id = "ram";
    mc->min_cpus = FBY35_MACHINE_NR_CPUS;
    mc->max_cpus = FBY35_MACHINE_NR_CPUS;
    mc->default_cpus = FBY35_MACHINE_NR_CPUS;
    mc->default_ram_size = FBY35_MACHINE_RAM_SIZE;
}

static const TypeInfo fby35_types[] = {
    {
        .name = TYPE_FBY35_MACHINE,
        .parent = TYPE_MACHINE,
        .class_init = fby35_machine_class_init,
        .instance_size = sizeof(Fby35MachineState),
    },
    {
        .name = TYPE_FBY35_SYSTEM_BUS,
        .parent = TYPE_SYS_BUS_DEVICE,
    },
};

DEFINE_TYPES(fby35_types);
