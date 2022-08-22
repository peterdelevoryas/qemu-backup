/*
 * QEMU model of the SPI GPIO controller
 *
 * Copyright (C)
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

#ifndef SPI_GPIO_H
#define SPI_GPIO_H

#include "qemu/osdep.h"
#include "hw/ssi/ssi.h"
#include "hw/gpio/aspeed_gpio.h"

#define TYPE_SPI_GPIO "spi_gpio"
OBJECT_DECLARE_SIMPLE_TYPE(SpiGpioState, SPI_GPIO);

struct SpiGpioState {
    SysBusDevice parent;
    SSIBus *spi;
    AspeedGPIOState *aspeed_gpio;

    int mode;
    int clk_counter;

    bool CIDLE, CPHA;
    uint32_t output_byte;
    uint32_t input_byte;

    bool clk, cs, miso;
    qemu_irq miso_output_pin, cs_output_pin;
};

#endif /* SPI_GPIO_H */
