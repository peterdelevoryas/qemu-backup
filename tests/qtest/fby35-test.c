/*
 * fby35 tests
 *
 * Copyright (c) Meta Platforms, Inc. and affiliates. (http://www.meta.com)
 *
 * This code is licensed under the GPL version 2 or later. See the COPYING
 * file in the top-level directory.
 */

#include "qemu/osdep.h"
#include "libqtest.h"
#include "hw/i2c/aspeed_i2c.h"

#define I2C_BASE 0x1E78A000
#define I2C_BUS0 (I2C_BASE + 0x80)
#define BMC_CPU 0
#define BIC_CPU 2

static void aspeed_i2c_init(QTestState *s)
{
    uint32_t v;

    qtest_cpu_write(s, BMC_CPU, I2C_BASE + A_I2C_CTRL_GLOBAL, 0, sizeof(uint32_t));

    v = qtest_readl(s, I2C_BUS0 + A_I2CD_FUN_CTRL);
    v = SHARED_FIELD_DP32(v, MASTER_EN, 1);
    qtest_cpu_write(s, BMC_CPU, I2C_BUS0 + A_I2CD_FUN_CTRL, v, sizeof(uint32_t));

    v = qtest_readl(s, I2C_BUS0 + A_I2CD_INTR_CTRL);
    v = SHARED_FIELD_DP32(v, TX_ACK, 1);
    v = SHARED_FIELD_DP32(v, TX_NAK, 1);
    v = SHARED_FIELD_DP32(v, RX_DONE, 1);
    v = SHARED_FIELD_DP32(v, NORMAL_STOP, 1);
    v = SHARED_FIELD_DP32(v, ABNORMAL, 1);
    v = SHARED_FIELD_DP32(v, SCL_TIMEOUT, 1);
    qtest_cpu_write(s, BMC_CPU, I2C_BUS0 + A_I2CD_INTR_CTRL, v, sizeof(uint32_t));
}

static void aspeed_i2c_old_master_tx_start(QTestState *s, uint8_t slave_addr)
{
    uint32_t v;

    v = qtest_readl(s, I2C_BUS0 + A_I2CD_BYTE_BUF);
    v = SHARED_FIELD_DP32(v, TX_BUF, slave_addr << 1);
    qtest_cpu_write(s, 0, I2C_BUS0 + A_I2CD_BYTE_BUF, v, sizeof(uint32_t));

    v = SHARED_FIELD_DP32(0, M_START_CMD, 1);
    v = SHARED_FIELD_DP32(v, M_TX_CMD, 1);
    qtest_cpu_write(s, BMC_CPU, I2C_BUS0 + A_I2CD_CMD, v, sizeof(uint32_t));
}

static void test_old_master_tx(void)
{
    QTestState *s;

    s = qtest_init("-machine fby35");

    aspeed_i2c_init(s);
    aspeed_i2c_old_master_tx_start(s, 0x20);

    qtest_quit(s);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    qtest_add_func("ast2600/i2c/old_master_tx", test_old_master_tx);

    return g_test_run();
}
