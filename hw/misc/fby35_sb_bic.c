/*
 * fby35 Server Board Bridge Interconnect
 *
 * Copyright (c) Meta Platforms, Inc. and affiliates. (http://www.meta.com)
 *
 * This code is licensed under the GPL version 2 or later. See the COPYING
 * file in the top-level directory.
 */

#include "qemu/osdep.h"
#include "qemu/log.h"
#include "qemu/main-loop.h"
#include "hw/i2c/i2c.h"
#include "trace.h"

#define TYPE_FBY35_SB_BIC "fby35-sb-bic"
OBJECT_DECLARE_SIMPLE_TYPE(BICState, FBY35_SB_BIC);

struct BICState {
    I2CSlave parent_obj;

    I2CBus *bus;
    QEMUBH *bic_to_bmc_tx;
    int rx_len;
    int tx_len;
    int tx_pos;
    uint8_t rx_buf[512];
    uint8_t tx_buf[512];
};

static void bic_to_bmc_tx(void *opaque)
{
    BICState *s = opaque;
    I2CSlave *i2c = I2C_SLAVE(s);
    uint8_t source_addr = i2c->address;
    uint8_t target_addr;

    assert(s->bus->bh == s->bic_to_bmc_tx);

    switch (s->tx_pos) {
    case 0:
        target_addr = s->tx_buf[s->tx_pos++];
        trace_fby35_sb_bic_tx_start(source_addr, target_addr);
        if (i2c_start_send_async(s->bus, target_addr)) {
            trace_fby35_sb_bic_tx_fail(source_addr, target_addr);
            break;
        }
        return;
    default:
        if (s->tx_pos >= s->tx_len) {
            break;
        }
        if (i2c_send_async(s->bus, s->tx_buf[s->tx_pos++])) {
            break;
        }
        return;
    }

    i2c_end_transfer(s->bus);
    i2c_bus_release(s->bus);
    s->tx_len = 0;
    s->tx_pos = 0;
    memset(s->tx_buf, 0, sizeof(s->tx_buf));
}

static void bic_realize(DeviceState *dev, Error **errp)
{
    BICState *s = FBY35_SB_BIC(dev);

    s->bus = I2C_BUS(qdev_get_parent_bus(dev));
    s->bic_to_bmc_tx = qemu_bh_new(bic_to_bmc_tx, s);
    s->rx_len = 0;
    s->tx_len = 0;
    s->tx_pos = 0;
    memset(s->rx_buf, 0, sizeof(s->rx_buf));
    memset(s->tx_buf, 0, sizeof(s->tx_buf));
}

static int bic_i2c_event(I2CSlave *i2c, enum i2c_event event)
{
    BICState *s = FBY35_SB_BIC(i2c);

    switch (event) {
    case I2C_START_SEND:
        trace_fby35_sb_bic_rx_start(i2c->address);
        s->rx_len = 0;
        memset(s->rx_buf, 0, sizeof(s->rx_buf));
        break;
    case I2C_FINISH:
        trace_fby35_sb_bic_rx_end(i2c->address);

        printf("BIC received message from BMC: [");
        for (int i = 0; i < s->rx_len; i++) {
            if (i) printf(" ");
            printf("%02x", s->rx_buf[i]);
        }
        printf("]\n");

        s->tx_pos = 0;
        s->tx_len = 0;
        s->tx_buf[s->tx_len++] = 0x10;
        s->tx_buf[s->tx_len++] = 0xde;
        s->tx_buf[s->tx_len++] = 0xad;
        s->tx_buf[s->tx_len++] = 0xbe;
        s->tx_buf[s->tx_len++] = 0xef;
        i2c_bus_master(s->bus, s->bic_to_bmc_tx);
        break;
    case I2C_START_SEND_ASYNC:
    case I2C_START_RECV:
    case I2C_NACK:
        qemu_log_mask(LOG_GUEST_ERROR, "%s: unexpected event: %d\n", __func__,
                      event);
        return -1;
    }

    return 0;
}

static uint8_t bic_i2c_recv(I2CSlave *i2c)
{
    qemu_log_mask(LOG_GUEST_ERROR, "%s: unexpected slave rx\n", __func__);
    return 0xff;
}

static int bic_i2c_send(I2CSlave *i2c, uint8_t data)
{
    BICState *s = FBY35_SB_BIC(i2c);

    trace_fby35_sb_bic_rx_data(i2c->address, data);

    assert(s->rx_len < sizeof(s->rx_buf));
    s->rx_buf[s->rx_len++] = data;

    return 0;
}

static void bic_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);
    I2CSlaveClass *i2c = I2C_SLAVE_CLASS(oc);

    dc->realize = bic_realize;
    i2c->event = bic_i2c_event;
    i2c->recv = bic_i2c_recv;
    i2c->send = bic_i2c_send;
}

static const TypeInfo types[] = {
    {
        .name = TYPE_FBY35_SB_BIC,
        .parent = TYPE_I2C_SLAVE,
        .instance_size = sizeof(BICState),
        .class_init = bic_class_init,
    },
};

DEFINE_TYPES(types);
