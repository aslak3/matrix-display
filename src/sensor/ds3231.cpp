#include "sensor.h"
#include "i2c.h"

#define DS3231_I2C_ADDR 0x68

Sensor *DS3231Sensor::create(const char *name)
{
    return new DS3231Sensor(name);
}

TemperatureSensor *DS3231Sensor::temperature_sensor(void)
{
    return this;
}

bool DS3231Sensor::receive_data(void)
{
    uint8_t val = 0x11; // device address (memory location) to read from

    return i2c_write_read(DS3231_I2C_ADDR, &val, 1, buffer, DS3231_TEMPERATURE_LEN);
}

float DS3231Sensor::get_temperature(void)
{
    return (float)(buffer[0]) + ((float)(buffer[1] >> 6) / 4.0);
}
