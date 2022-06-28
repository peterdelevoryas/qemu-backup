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
#include "hw/i2c/i2c.h"
#include "hw/registerfields.h"

#define BOARD_ID_CLASS1 0b0000
#define BOARD_ID_CLASS2 0b0001

#define TYPE_FBY35_CPLD "fby35-cpld"
OBJECT_DECLARE_SIMPLE_TYPE(Fby35CpldState, FBY35_CPLD);

REG8(CLASS_TYPE, 0x5);
    FIELD(CLASS_TYPE, RESERVED, 0, 2);
    FIELD(CLASS_TYPE, 1OU_EXPANSION_NOT_PRESENT, 2, 1);
    FIELD(CLASS_TYPE, 2OU_EXPANSION_NOT_PRESENT, 3, 1);
    FIELD(CLASS_TYPE, BOARD_ID, 4, 4);
REG8(BOARD_REVISION, 0x8);
    FIELD(BOARD_REVISION, VALUE, 0, 4);
    FIELD(BOARD_REVISION, RESERVED, 4, 4);

struct Fby35CpldState {
    I2CSlave parent_obj;

    uint8_t target_reg;
    uint32_t regs[10];
};

static void fby35_cpld_realize(DeviceState *dev, Error **errp)
{
    Fby35CpldState *s = FBY35_CPLD(dev);

    memset(s->regs, 0, sizeof(s->regs));
    s->target_reg = 0;

    ARRAY_FIELD_DP32(s->regs, CLASS_TYPE, BOARD_ID, 0b0000);
    ARRAY_FIELD_DP32(s->regs, CLASS_TYPE, 1OU_EXPANSION_NOT_PRESENT, 1);
    ARRAY_FIELD_DP32(s->regs, CLASS_TYPE, 2OU_EXPANSION_NOT_PRESENT, 1);
    ARRAY_FIELD_DP32(s->regs, BOARD_REVISION, VALUE, 0x1);
}

static int fby35_cpld_i2c_event(I2CSlave *i2c, enum i2c_event event)
{
    Fby35CpldState *s = FBY35_CPLD(i2c);

    switch (event) {
    case I2C_START_RECV:
        break;
    case I2C_START_SEND:
        s->target_reg = 0;
        break;
    case I2C_START_SEND_ASYNC:
    case I2C_FINISH:
    case I2C_NACK:
        break;
    }

    return 0;
}

static uint8_t fby35_cpld_i2c_recv(I2CSlave *i2c)
{
    Fby35CpldState *s = FBY35_CPLD(i2c);

    switch (s->target_reg) {
    case R_CLASS_TYPE:
    case R_BOARD_REVISION:
        return s->regs[s->target_reg];
    default:
        printf("%s: unexpected register read 0x%02x\n", __func__, s->target_reg);
        return 0xff;
    }
}

static int fby35_cpld_i2c_send(I2CSlave *i2c, uint8_t data)
{
    Fby35CpldState *s = FBY35_CPLD(i2c);

    if (s->target_reg == 0) {
        s->target_reg = data;
        return 0;
    }

    switch (s->target_reg) {
    case R_CLASS_TYPE:
    case R_BOARD_REVISION:
        s->regs[s->target_reg] = data;
        break;
    default:
        printf("%s: unexpected register write 0x%02x 0x%02x\n", __func__, s->target_reg, data);
        break;
    }

    return 0;
}

static void fby35_cpld_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);
    I2CSlaveClass *i2c = I2C_SLAVE_CLASS(oc);

    dc->realize = fby35_cpld_realize;
    i2c->event = fby35_cpld_i2c_event;
    i2c->recv = fby35_cpld_i2c_recv;
    i2c->send = fby35_cpld_i2c_send;
}

static const TypeInfo types[] = {
    {
        .name = TYPE_FBY35_CPLD,
        .parent = TYPE_I2C_SLAVE,
        .instance_size = sizeof(Fby35CpldState),
        .class_init = fby35_cpld_class_init,
    },
};

DEFINE_TYPES(types);
