#include <stdio.h>
#include <stdlib.h>

#if PICO_SDK
#include <pico/cyw43_arch.h>

#include <hardware/i2c.h>
#endif

#define DS3231_I2C_ADDR 0x68

#define DS3231_TEMPERATURE_LEN 2

extern bool got_data;

float get_temperature(void)
{
    uint8_t buffer[DS3231_TEMPERATURE_LEN];

    uint8_t val = 0x11; // device address to read from
#if PCIO_SDK
    // true to keep master control of bus
    i2c_write_blocking(i2c_default, DS3231_I2C_ADDR, &val, 1, true);
    i2c_read_blocking(i2c_default, DS3231_I2C_ADDR, buffer, DS3231_TEMPERATURE_LEN, false);
#else
    buffer[0] = val;
    buffer[1] = val;
#endif

    return (float)(buffer[0]) + ((float)(buffer[1] >> 6) / 4.0);
}
