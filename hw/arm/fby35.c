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
#include "hw/boards.h"

#define FBY35_MACHINE_NR_CPUS (1)

#define TYPE_FBY35_MACHINE MACHINE_TYPE_NAME("fby35")
OBJECT_DECLARE_SIMPLE_TYPE(Fby35MachineState, FBY35_MACHINE);

struct Fby35MachineState {
    MachineState parent_obj;
};

static void fby35_machine_init(MachineState *machine)
{
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
    mc->default_ram_size = 0;
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
