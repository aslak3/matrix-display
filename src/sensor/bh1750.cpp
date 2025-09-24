#include "sensor.h"

#include "i2c.h"

#define BH1750_I2C_ADDR 0x23

Sensor *BH1750Sensor::create(const char *name)
{
    return new BH1750Sensor(name);
}

IlluminanceSensor *BH1750Sensor::illuminance_sensor(void)
{
    return this;
}

bool BH1750Sensor::request_run(void)
{
    uint8_t high_res_instruction = 0x10;

    return i2c_write(BH1750_I2C_ADDR, &high_res_instruction, 1);
}

bool BH1750Sensor::receive_data(void)
{
    return i2c_read(BH1750_I2C_ADDR, buffer, BH1750_ILLUMINANCE_LEN);
}

uint16_t BH1750Sensor::get_illuminance(void)
{
    return (buffer[0] * 256) + buffer[1];
}
