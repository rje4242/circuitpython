/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Scott Shawcroft
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
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

 // This file contains all of the port specific HAL functions for the machine
 // module.

#include "shared-bindings/nativeio/SPI.h"
#include "py/nlr.h"

// We use ENABLE registers below we don't want to treat as a macro.
#undef ENABLE

// Number of times to try to send packet if failed.
#define TIMEOUT 1

void common_hal_nativeio_spi_construct(nativeio_spi_obj_t *self,
        const mcu_pin_obj_t * clock, const mcu_pin_obj_t * mosi,
        const mcu_pin_obj_t * miso, uint32_t baudrate) {
    struct spi_config config_spi_master;
    spi_get_config_defaults(&config_spi_master);

    Sercom* sercom = NULL;
    uint32_t clock_pinmux = 0;
    uint32_t mosi_pinmux = 0;
    uint32_t miso_pinmux = 0;
    uint8_t clock_pad = 0;
    uint8_t mosi_pad = 0;
    uint8_t miso_pad = 0;
    for (int i = 0; i < NUM_SERCOMS_PER_PIN; i++) {
        Sercom* potential_sercom = clock->sercom[i].sercom;
        if (potential_sercom == NULL ||
            potential_sercom->SPI.CTRLA.bit.ENABLE != 0) {
            continue;
        }
        clock_pinmux = clock->sercom[i].pinmux;
        clock_pad = clock->sercom[i].pad;
        for (int j = 0; j < NUM_SERCOMS_PER_PIN; j++) {
            mosi_pinmux = mosi->sercom[j].pinmux;
            mosi_pad = mosi->sercom[j].pad;
            for (int k = 0; k < NUM_SERCOMS_PER_PIN; k++) {
                if (potential_sercom == miso->sercom[k].sercom) {
                    miso_pinmux = miso->sercom[k].pinmux;
                    miso_pad = miso->sercom[k].pad;
                    sercom = potential_sercom;
                    break;
                }
            }
            if (sercom != NULL) {
                break;
            }
        }
        if (sercom != NULL) {
            break;
        }
    }
    if (sercom == NULL) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError,
            "No hardware support available with those pins."));
    }

    // Depends on where MOSI and CLK are.
    uint8_t dopo = 8;
    if (clock_pad == 1) {
        if (mosi_pad == 0) {
            dopo = 0;
        } else if (mosi_pad == 3) {
            dopo = 2;
        }
    } else if (clock_pad == 3) {
        if (mosi_pad == 0) {
            dopo = 3;
        } else if (mosi_pad == 2) {
            dopo = 1;
        }
    }
    if (dopo == 8) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "SPI MOSI and clock pins incompatible."));
    }

    config_spi_master.mux_setting = (dopo << SERCOM_SPI_CTRLA_DOPO_Pos) |
        (miso_pad << SERCOM_SPI_CTRLA_DIPO_Pos);

    // Map pad to pinmux through a short array.
    uint32_t *pinmuxes[4] = {&config_spi_master.pinmux_pad0,
                             &config_spi_master.pinmux_pad1,
                             &config_spi_master.pinmux_pad2,
                             &config_spi_master.pinmux_pad3};
    *pinmuxes[clock_pad] = clock_pinmux;
    *pinmuxes[mosi_pad] = mosi_pinmux;
    *pinmuxes[miso_pad] = miso_pinmux;

    config_spi_master.mode_specific.master.baudrate = baudrate;

    spi_init(&self->spi_master_instance, sercom, &config_spi_master);

    spi_enable(&self->spi_master_instance);
}

void common_hal_nativeio_spi_deinit(nativeio_spi_obj_t *self) {
    spi_disable(&self->spi_master_instance);
}

bool common_hal_nativeio_spi_write(nativeio_spi_obj_t *self,
        const uint8_t *data, size_t len) {
    enum status_code status = spi_write_buffer_wait(
        &self->spi_master_instance,
        data,
        len);
    return status == STATUS_OK;
}

bool common_hal_nativeio_spi_read(nativeio_spi_obj_t *self,
        uint8_t *data, size_t len) {
    enum status_code status = spi_read_buffer_wait(
        &self->spi_master_instance,
        data,
        len,
        0);
    return status == STATUS_OK;
}