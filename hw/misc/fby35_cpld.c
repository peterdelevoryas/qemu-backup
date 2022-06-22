#include "qemu/osdep.h"
#include "qemu/main-loop.h"
#include "qapi/error.h"
#include "hw/i2c/i2c.h"
#include "hw/qdev-properties.h"

#define TYPE_FBY35_CPLD "fby35-cpld"
OBJECT_DECLARE_SIMPLE_TYPE(Fby35Cpld, FBY35_CPLD);

typedef struct Fby35Cpld FBy35Cpld;

struct Fby35Cpld {
    I2CSlave parent;

    I2CBus *bus;
};

static void fby35_cpld_realize(DeviceState *dev, Error **errp)
{
}

static int fby35_cpld_i2c_event(I2CSlave *i2c, enum i2c_event event)
{
    return 0;
}

static uint8_t fby35_cpld_i2c_recv(I2CSlave *i2c)
{
    return 0xff;
}

static int fby35_cpld_i2c_send(I2CSlave *i2c, uint8_t byte)
{
    //Fby35Cpld *s = FBY35_CPLD(i2c);

    return 0;
}

static void fby35_cpld_class_init(ObjectClass *cls, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(cls);
    dc->realize = fby35_cpld_realize;

    I2CSlaveClass *sc = I2C_SLAVE_CLASS(cls);
    sc->event = fby35_cpld_i2c_event;
    sc->recv = fby35_cpld_i2c_recv;
    sc->send = fby35_cpld_i2c_send;
}

static const TypeInfo fby35_cpld = {
    .name = TYPE_FBY35_CPLD,
    .parent = TYPE_I2C_SLAVE,
    .instance_size = sizeof(Fby35Cpld),
    .class_init = fby35_cpld_class_init,
};

static void register_types(void)
{
    type_register_static(&fby35_cpld);
}

type_init(register_types);
