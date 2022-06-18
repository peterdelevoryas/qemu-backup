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
#include "hw/boards.h"
#include "hw/qdev-properties.h"
#include "hw/arm/aspeed_soc.h"

#define FBY35_BMC_NR_CPUS (2)
#define FBY35_BMC_RAM_SIZE (2 * GiB)
#define FBY35_BMC_HW_STRAP1 (0x000000C0)
#define FBY35_BMC_HW_STRAP2 (0x00000003)

#define FBY35_MACHINE_NR_CPUS (FBY35_BMC_NR_CPUS)
#define FBY35_MACHINE_RAM_SIZE (FBY35_BMC_RAM_SIZE)

#define TYPE_FBY35_MACHINE MACHINE_TYPE_NAME("fby35")
OBJECT_DECLARE_SIMPLE_TYPE(Fby35MachineState, FBY35_MACHINE);

struct Fby35MachineState {
    MachineState parent_obj;

    MemoryRegion bmc_system_memory;
    MemoryRegion bmc_dram;
    MemoryRegion bmc_boot_rom;

    AspeedSoCState bmc;
};

static void fby35_bmc_init(MachineState *machine)
{
    Fby35MachineState *s = FBY35_MACHINE(machine);
    AspeedSoCClass *sc = NULL;

    memory_region_init(&s->bmc_system_memory, OBJECT(s), "bmc-system-memory", UINT64_MAX);
    memory_region_init(&s->bmc_dram, OBJECT(s), "bmc-dram", FBY35_BMC_RAM_SIZE);
    memory_region_add_subregion(get_system_memory(), 0, &s->bmc_system_memory);
    memory_region_add_subregion(&s->bmc_dram, 0, machine->ram);

    object_initialize_child(OBJECT(s), "bmc", &s->bmc, "ast2600-a3");
    object_property_set_int(OBJECT(&s->bmc), "ram-size", FBY35_BMC_RAM_SIZE, &error_abort);
    object_property_set_link(OBJECT(&s->bmc), "system-memory", OBJECT(&s->bmc_system_memory), &error_abort);
    object_property_set_link(OBJECT(&s->bmc), "dram", OBJECT(&s->bmc_dram), &error_abort);
    qdev_prop_set_uint32(DEVICE(&s->bmc), "hw-strap1", FBY35_BMC_HW_STRAP1);
    qdev_prop_set_uint32(DEVICE(&s->bmc), "hw-strap2", FBY35_BMC_HW_STRAP2);
    qdev_prop_set_uint32(DEVICE(&s->bmc), "uart-default", ASPEED_DEV_UART5);
    qdev_realize(DEVICE(&s->bmc), NULL, &error_abort);
    sc = ASPEED_SOC_GET_CLASS(&s->bmc);

    memory_region_add_subregion(&s->bmc_system_memory, sc->memmap[ASPEED_DEV_SDRAM], &s->bmc_dram);
    memory_region_init_rom(&s->bmc_boot_rom, OBJECT(s), "bmc-boot-rom", 128 * MiB, &error_abort);
    memory_region_add_subregion(&s->bmc_system_memory, 0, &s->bmc_boot_rom);

    aspeed_board_init_flashes(&s->bmc.fmc, "n25q00", 2, 0);
}

static void fby35_machine_init(MachineState *machine)
{
    fby35_bmc_init(machine);
}

static void fby35_machine_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    mc->desc = "Meta Platforms fby35";
    mc->init = fby35_machine_init;
    mc->no_floppy = 1;
    mc->no_cdrom = 1;
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
};

DEFINE_TYPES(fby35_types);
