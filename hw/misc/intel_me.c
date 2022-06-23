#include "qemu/osdep.h"
#include "qemu/main-loop.h"
#include "hw/i2c/i2c.h"

#define TYPE_INTEL_ME "intel-me"
OBJECT_DECLARE_SIMPLE_TYPE(IntelME, INTEL_ME);

enum State {
    IDLE,
    RX,
    TX,
};

struct IntelME {
    I2CSlave parent;

    enum State state;
    I2CBus *bus;
    QEMUBH *bh;
    uint8_t rx_len;
    uint8_t tx_len;
    uint8_t buf[64];
};

static void intel_me_bh(void *opaque)
{
    IntelME *s = opaque;

    printf("INTEL ME BH\n");

    switch (s->state) {
    case IDLE:
    case RX:
        abort();
    case TX:
        if (s->tx_len == 0) {
            printf("INTEL ME START SEND TO 0x%02x\n", s->buf[0]);
            i2c_start_send(s->bus, s->buf[s->tx_len++]);
            break;
        }
        if (s->tx_len < s->rx_len) {
            printf("INTEL ME SEND ASYNC 0x%02x\n", s->buf[s->tx_len]);
            i2c_send_async(s->bus, s->buf[s->tx_len++]);
            break;
        }
        printf("INTEL ME END TRANSFER\n");
        i2c_end_transfer(s->bus);
        i2c_bus_release(s->bus);
        break;
    }
}

static void intel_me_realize(DeviceState *dev, Error **errp)
{
    IntelME *s = INTEL_ME(dev);
    I2CBus *bus = I2C_BUS(qdev_get_parent_bus(dev));

    s->state = IDLE;
    s->bus = bus;
    s->bh = qemu_bh_new(intel_me_bh, s);
    s->rx_len = 0;
    s->tx_len = 0;
    memset(s->buf, 0, sizeof(s->buf));
}

static int intel_me_i2c_event(I2CSlave *i2c, enum i2c_event event)
{
    IntelME *s = INTEL_ME(i2c);

    switch (event) {
    case I2C_START_RECV:
        printf("INTEL ME RECV UNIMPLEMENTED\n");
        abort();
    case I2C_START_SEND:
        s->state = RX;
        s->rx_len = 0;
        memset(s->buf, 0, sizeof(s->buf));
        break;
    case I2C_FINISH:
        assert(s->state == RX);
        s->state = TX;
        s->tx_len = 0;
        printf("first byte 0x%02x\n", s->buf[0]);
        s->buf[0] = 0x20;
        i2c_bus_master(s->bus, s->bh);
        break;
    case I2C_NACK:
        printf("INTEL ME I2C NACK???\n");
        abort();
    }
    return 0;
}

static int intel_me_i2c_send(I2CSlave *i2c, uint8_t byte)
{
    IntelME *s = INTEL_ME(i2c);

    switch (s->state) {
    case RX:
        printf("INTEL ME RX 0x%02x\n", byte);
        s->buf[s->rx_len++] = byte;
        break;
    case IDLE:
    case TX:
        abort();
    }
    return 0;
}

static void intel_me_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);
    dc->realize = intel_me_realize;

    I2CSlaveClass *sc = I2C_SLAVE_CLASS(oc);
    sc->event = intel_me_i2c_event;
    sc->send = intel_me_i2c_send;
}

static const TypeInfo intel_me = {
    .name = TYPE_INTEL_ME,
    .parent = TYPE_I2C_SLAVE,
    .instance_size = sizeof(IntelME),
    .class_init = intel_me_class_init,
};

static void register_types(void)
{
    type_register_static(&intel_me);
}

type_init(register_types);
