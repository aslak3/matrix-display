#include "i2c.h"

#define DS3231_I2C_ADDR 0x68

#define DS3231_TEMPERATURE_LEN 2

float get_temperature(void)
{
    uint8_t buffer[DS3231_TEMPERATURE_LEN];

    uint8_t val = 0x11; // device address (memory location) to read from

    i2c_write_read(DS3231_I2C_ADDR, &val, 1, buffer, DS3231_TEMPERATURE_LEN);

    return (float)(buffer[0]) + ((float)(buffer[1] >> 6) / 4.0);
}
