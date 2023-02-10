/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "qemu/osdep.h"
#include "qemu/main-loop.h"
#include "qemu/log.h"
#include "hw/i2c/i2c.h"
#include "hw/qdev-properties.h"
#include "net/net.h"

#define MAX_I2C_FRAME_SIZE 128
#define TYPE_I2C_TUNNEL "i2c-tunnel"

OBJECT_DECLARE_SIMPLE_TYPE(I2CTunnel, I2C_TUNNEL);

struct I2CTunnel {
    /*< private >*/
    I2CSlave parent;

    /*< public >*/
    I2CBus *bus;
    NICState *nic;
    NICConf conf;
    QEMUBH *tx_task;
    size_t tx_pos;
    size_t tx_len;
    size_t rx_len;
    uint8_t tx_buf[MAX_I2C_FRAME_SIZE];
    uint8_t rx_buf[MAX_I2C_FRAME_SIZE];
};

static bool i2c_tunnel_nic_can_receive(NetClientState *nc)
{
    I2CTunnel *s = I2C_TUNNEL(qemu_get_nic_opaque(nc));

    return s->tx_len == 0;
}

static ssize_t i2c_tunnel_nic_receive(NetClientState *nc, const uint8_t *buf,
                                      size_t len)
{
    I2CTunnel *s = I2C_TUNNEL(qemu_get_nic_opaque(nc));

    // printf("%s: ", __func__);
    // for (int i = 0; i < len; i++) {
    //     printf("%02x ", buf[i]);
    // }
    // printf("\n");

    memcpy(s->tx_buf, buf, len);
    s->tx_pos = 0;
    s->tx_len = len;

    qemu_bh_schedule(s->tx_task);

    return len;
}

static void i2c_tunnel_tx_task(void *opaque)
{
    I2CTunnel *s = opaque;
    uint8_t b;

    // printf("%s: tx_pos=%ld tx_len=%ld\n", __func__, s->tx_pos, s->tx_len);

    if (s->bus->bh != s->tx_task) {
        assert(s->tx_pos == 0);
        assert(s->tx_len);
        i2c_bus_master(s->bus, s->tx_task);
        return;
    }

    if (s->tx_pos == 0) {
        b = s->tx_buf[s->tx_pos++];
        if (b & 1) {
            printf("%s: tunnel clients are only allowed to send data for now\n",
                   TYPE_I2C_TUNNEL);
            goto bus_release;
        }
        if (i2c_start_send_async(s->bus, b >> 1)) {
            printf("%s: no device ack'd start at address 0x%02x\n",
                   TYPE_I2C_TUNNEL, b >> 1);
            goto bus_release;
        }
        /* Wait for target to ack start condition. */
        return;
    }

    if (s->tx_pos < s->tx_len) {
        b = s->tx_buf[s->tx_pos++];
        if (i2c_send_async(s->bus, b)) {
            printf("%s: Error sending to target 0x%02x\n", TYPE_I2C_TUNNEL,
                   s->tx_buf[0] >> 1);
            goto end_transfer;
        }
        /* Wait for target to ack byte. */
        return;
    }

end_transfer:
    i2c_end_transfer(s->bus);
bus_release:
    i2c_bus_release(s->bus);

    s->tx_pos = 0;
    s->tx_len = 0;
    memset(s->tx_buf, 0, sizeof(s->tx_buf));
    /* Unblock NIC rx. */
    qemu_flush_or_purge_queued_packets(qemu_get_queue(s->nic), false);
}

static void i2c_tunnel_nic_cleanup(NetClientState *nc)
{
    I2CTunnel *s = I2C_TUNNEL(qemu_get_nic_opaque(nc));

    s->nic = NULL;
}

static NetClientInfo nic_info = {
    .type = NET_CLIENT_DRIVER_NIC,
    .size = sizeof(NICState),
    .can_receive = i2c_tunnel_nic_can_receive,
    .receive = i2c_tunnel_nic_receive,
    .cleanup = i2c_tunnel_nic_cleanup,
};

static void i2c_tunnel_realize(DeviceState *dev, Error **errp)
{
    I2CTunnel *s = I2C_TUNNEL(dev);

    s->bus = I2C_BUS(qdev_get_parent_bus(dev));
    s->nic = qemu_new_nic(&nic_info, &s->conf, TYPE_I2C_TUNNEL, dev->id, s);
    s->tx_task = qemu_bh_new(i2c_tunnel_tx_task, s);
}

static const char *i2c_event_string(enum i2c_event event)
{
    switch (event) {
    case I2C_START_RECV:
        return "start-recv";
    case I2C_START_SEND:
        return "start-send";
    case I2C_START_SEND_ASYNC:
        return "start-send-async";
    case I2C_FINISH:
        return "finish";
    case I2C_NACK:
        return "nack";
    default:
        return NULL;
    }
}

static int i2c_tunnel_event(I2CSlave *i2c, enum i2c_event event)
{
    I2CTunnel *s = I2C_TUNNEL(i2c);

    //printf("%s: i2c event %s\n", TYPE_I2C_TUNNEL, i2c_event_string(event));

    switch (event) {
    case I2C_START_RECV:
    case I2C_START_SEND_ASYNC:
    case I2C_NACK:
        qemu_log_mask(LOG_UNIMP, "%s: %s unimplemented\n", TYPE_I2C_TUNNEL,
                      i2c_event_string(event));
        break;
    case I2C_START_SEND:
        memset(s->rx_buf, 0, sizeof(s->rx_buf));
        s->rx_len = 0;
        s->rx_buf[s->rx_len++] = i2c->address << 1;
        //printf("%s: i2c address 0x%02x\n", TYPE_I2C_TUNNEL, i2c->address << 1);
        break;
    case I2C_FINISH:
        qemu_send_packet(qemu_get_queue(s->nic), s->rx_buf, s->rx_len);
        memset(s->rx_buf, 0, sizeof(s->rx_buf));
        s->rx_len = 0;
        break;
    }

    return 0;
}

static int i2c_tunnel_send(I2CSlave *i2c, uint8_t data)
{
    I2CTunnel *s = I2C_TUNNEL(i2c);

    if (s->rx_len + 1 > sizeof(s->rx_buf)) {
        qemu_log_mask(LOG_GUEST_ERROR, "%s: transmit overflow: %zu/%zu\n",
                      TYPE_I2C_TUNNEL, s->rx_len + 1, sizeof(s->rx_buf));
        return 0;
    }

    //printf("%s: i2c rx 0x%02x\n", TYPE_I2C_TUNNEL, data);

    s->rx_buf[s->rx_len++] = data;
    return 0;
}

static uint8_t i2c_tunnel_recv(I2CSlave *i2c)
{
    /* Slave receive unimplemented */
    return 0xff;
}

static Property i2c_tunnel_props[] = {
    DEFINE_NIC_PROPERTIES(I2CTunnel, conf),
    DEFINE_PROP_END_OF_LIST(),
};

static void i2c_tunnel_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    I2CSlaveClass *sc = I2C_SLAVE_CLASS(klass);

    dc->realize = i2c_tunnel_realize;
    sc->event = i2c_tunnel_event;
    sc->send = i2c_tunnel_send;
    sc->recv = i2c_tunnel_recv;

    device_class_set_props(dc, i2c_tunnel_props);
}

static const TypeInfo i2c_tunnel_types[] = {
    {
        .name          = TYPE_I2C_TUNNEL,
        .parent        = TYPE_I2C_SLAVE,
        .class_init    = i2c_tunnel_class_init,
        .instance_size = sizeof(I2CTunnel),
    },
};

DEFINE_TYPES(i2c_tunnel_types);
