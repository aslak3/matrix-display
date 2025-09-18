// Portions of this file, specifically the get_temperature, get_pressure and get_humidity
// subroutines are based on the BM68x SensorAPI:

/**
* Copyright (c) 2023 Bosch Sensortec GmbH. All rights reserved.
*
* BSD-3-Clause
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* 1. Redistributions of source code must retain the above copyright
*    notice, this list of conditions and the following disclaimer.
*
* 2. Redistributions in binary form must reproduce the above copyright
*    notice, this list of conditions and the following disclaimer in the
*    documentation and/or other materials provided with the distribution.
*
* 3. Neither the name of the copyright holder nor the names of its
*    contributors may be used to endorse or promote products derived from
*    this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
* "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
* FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
* COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
* SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
* STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
* IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
* POSSIBILITY OF SUCH DAMAGE.
*
* @file       bme68x.c
* @date       2023-02-07
* @version    v4.4.8
*
*/

#include <string.h>

#include "i2c.h"

#define BME680_I2C_ADDR 0x76

#define BME680_CTRL_HUM 0x72        // osrs_h>2:0>
#define BME680_CTRL_MEAS 0x74       // osrs_t<7:5> osrs_p<4:2>

#define CALIB_PAR_T1_LSB (33)
#define CALIB_PAR_T1_MSB (34)
#define CALIB_PAR_T2_LSB (1)
#define CALIB_PAR_T2_MSB (2)
#define CALIB_PAR_T3 (3)

#define CALIB_PAR_P10 (23)
#define CALIB_PAR_P9_LSB (21)
#define CALIB_PAR_P9_MSB (22)
#define CALIB_PAR_P8_LSB (19)
#define CALIB_PAR_P8_MSB (20)
#define CALIB_PAR_P7 (15)
#define CALIB_PAR_P6 (16)
#define CALIB_PAR_P5_LSB (13)
#define CALIB_PAR_P5_MSB (14)
#define CALIB_PAR_P4_LSB (11)
#define CALIB_PAR_P4_MSB (12)
#define CALIB_PAR_P3 (9)
#define CALIB_PAR_P2_LSB (7)
#define CALIB_PAR_P2_MSB (8)
#define CALIB_PAR_P1_LSB (5)
#define CALIB_PAR_P1_MSB (6)

#define CALIB_PAR_H2_MSB (25)
#define CALIB_PAR_H2_LSB (26)
#define CALIB_PAR_H1_MSB (27)
#define CALIB_PAR_H1_LSB (26)
#define CALIB_PAR_H3 (28)
#define CALIB_PAR_H4 (29)
#define CALIB_PAR_H5 (30)
#define CALIB_PAR_H6 (31)
#define CALIB_PAR_H7 (32)

static float t_fine = 0.0;
static uint8_t current_state[256];
static uint8_t calib[25 + 16];

// Returns 0 on success
int configure_bme680(void)
{
    int bytes_written = 0;

    uint8_t config_buffer[] =   {
        // 1. Set humidity oversampling to 1x by writing 0b001 to osrs_h<2:0>
        // 2. Set temperature oversampling to 2x by writing 0b010 to osrs_t<2:0>
        // 3. Set pressure oversampling to 16x by writing 0b101 to osrs_p<2:0*/
        BME680_CTRL_HUM, 0b00000001,
        BME680_CTRL_MEAS, 0b01010100,
    };

    if (i2c_write(BME680_I2C_ADDR, config_buffer, sizeof(config_buffer),
        false) != sizeof(config_buffer))
    {
        return 0;
    }

    return 1;
}

// Returns 1 on success
int receive_data(void)
{
    const uint8_t base_reg_addr = 0;

    if (i2c_write(BME680_I2C_ADDR, &base_reg_addr, 1, true) != 1) {
        return 1;
    }
    if (i2c_read(BME680_I2C_ADDR, current_state, sizeof(current_state),
        false) != sizeof(current_state))
    {
        return 0;
    }

    memcpy(&calib[0], &current_state[0x89], 25);
    memcpy(&calib[25], &current_state[0xe1], 16);

    return 1;
}

// Returns 1 on success
int request_run(void)
{
    uint8_t run_buffer[] = { BME680_CTRL_MEAS, 0b01010101 };
    if (i2c_write(BME680_I2C_ADDR, run_buffer, sizeof(run_buffer), false) != sizeof(run_buffer)) {
        return 0;
    }

    return 1;
}

float get_temperature(void)
{
    uint32_t temp_adc = (current_state[0x22] << 12) | (current_state[0x23] << 4) | (current_state[0x24] >> 4);

    uint16_t par_t1 = (calib[CALIB_PAR_T1_MSB] << 8) | calib[CALIB_PAR_T1_LSB];
    int16_t par_t2 = (calib[CALIB_PAR_T2_MSB] << 8) | calib[CALIB_PAR_T2_LSB];
    int8_t par_t3 = calib[CALIB_PAR_T3];

    float var1 = ((((float)temp_adc / 16384.0f) - ((float)par_t1 / 1024.0f)) * ((float)par_t2));
    float var2 = (((((float)temp_adc / 131072.0f) - ((float)par_t1 / 8192.0f)) *
        (((float)temp_adc / 131072.0f) - ((float)par_t1 / 8192.0f))) * ((float)par_t3 * 16.0f));

    /* Export the t_fine value as it's used by preasure and humidty */
    t_fine = (var1 + var2);

    return (t_fine) / 5120.0f;
}

float get_pressure(void)
{
    uint8_t par_p10 = calib[CALIB_PAR_P10];
    int16_t par_p9 = (calib[CALIB_PAR_P9_MSB] << 8) | calib[CALIB_PAR_P9_LSB];
    int16_t par_p8 = (calib[CALIB_PAR_P8_MSB] << 8) | calib[CALIB_PAR_P8_LSB];
    int8_t par_p7 = calib[CALIB_PAR_P7];
    int8_t par_p6 = calib[CALIB_PAR_P6];
    int16_t par_p5 = (calib[CALIB_PAR_P5_MSB] << 8) | calib[CALIB_PAR_P5_LSB];
    int16_t par_p4 = (calib[CALIB_PAR_P4_MSB] << 8) | calib[CALIB_PAR_P4_LSB];
    int8_t par_p3 = calib[CALIB_PAR_P3];
    int16_t par_p2 = (calib[CALIB_PAR_P2_MSB] << 8) | calib[CALIB_PAR_P2_LSB];
    uint16_t par_p1 = (calib[CALIB_PAR_P1_MSB] << 8) | calib[CALIB_PAR_P1_LSB];

    uint32_t pres_adc = (current_state[0x1f] << 12) | (current_state[0x20] << 4) | (current_state[0x21] >> 4);

    float var1 = ((t_fine / 2.0f) - 64000.0f);
    float var2 = var1 * var1 * (((float)par_p6) / (131072.0f));
    var2 = var2 + (var1 * ((float)par_p5) * 2.0f);
    var2 = (var2 / 4.0f) + (((float)par_p4) * 65536.0f);
    var1 = (((((float)par_p3 * var1 * var1) / 16384.0f) + ((float)par_p2 * var1)) / 524288.0f);
    var1 = ((1.0f + (var1 / 32768.0f)) * ((float)par_p1));
    float calc_pres = (1048576.0f - ((float)pres_adc));

    /* Avoid panic caused by division by zero */
    if ((int)var1 != 0) {
        calc_pres = (((calc_pres - (var2 / 4096.0f)) * 6250.0f) / var1);
        var1 = (((float)par_p9) * calc_pres * calc_pres) / 2147483648.0f;
        var2 = calc_pres * (((float)par_p8) / 32768.0f);
        float var3 = ((calc_pres / 256.0f) * (calc_pres / 256.0f) * (calc_pres / 256.0f) * (par_p10 / 131072.0f));
        calc_pres = (calc_pres + (var1 + var2 + var3 + ((float)par_p7 * 128.0f)) / 16.0f);
    }
    else {
        calc_pres = 0.0;
    }

    return calc_pres / 100.0;
}

float get_humidity(void)
{
    int8_t par_h7 = calib[CALIB_PAR_H7];
    uint8_t par_h6 = calib[CALIB_PAR_H6];
    int8_t par_h5 = calib[CALIB_PAR_H5];
    int8_t par_h4 = calib[CALIB_PAR_H4];
    int8_t par_h3 = calib[CALIB_PAR_H3];
    uint16_t par_h2 = (calib[CALIB_PAR_H2_MSB] << 4) | calib[CALIB_PAR_H2_LSB] & 0x0f;
    uint16_t par_h1 = (calib[CALIB_PAR_H1_MSB] << 4) | calib[CALIB_PAR_H1_LSB] >> 4;

    uint32_t hum_adc = (current_state[0x25] << 8) | current_state[0x26];

    /* Compensated temperature data */
    float temp_comp = ((t_fine) / 5120.0f);
    float var1 = (float)((float)hum_adc) -
           (((float)par_h1 * 16.0f) + (((float)par_h3 / 2.0f) * temp_comp));
    float var2 = var1 * ((float)(((float)par_h2 / 262144.0f) *
            (1.0f + (((float)par_h4 / 16384.0f) * temp_comp) +
            (((float)par_h5 / 1048576.0f) * temp_comp * temp_comp))));
    float var3 = (float)par_h6 / 16384.0f;
    float var4 = (float)par_h7 / 2097152.0f;
    float calc_hum = var2 + ((var3 + (var4 * temp_comp)) * var2 * var2);

    /* Clamp between 0% and 100% */
    if (calc_hum > 100.0f) {
        calc_hum = 100.0f;
    }
    else if (calc_hum < 0.0f) {
        calc_hum = 0.0f;
    }

    return calc_hum;
}
