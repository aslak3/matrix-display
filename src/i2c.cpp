#include <stdlib.h>

#if PICO_SDK
#include <hardware/i2c.h>
#include <hardware/gpio.h>
#elif ESP32_SDK
#include "driver/i2c.h"
#endif

#if PICO_SDK

#define PICO_I2C_SDA_PIN 20
#define PICO_I2C_SCL_PIN 21

void setup_i2c(void)
{
    // This example will use I2C0 on the default SDA and SCL pins (4, 5 on a Pico)
    i2c_init(i2c_default, 100 * 1000);
    gpio_set_function(PICO_I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(PICO_I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(PICO_I2C_SDA_PIN);
    gpio_pull_up(PICO_I2C_SCL_PIN);
}

bool i2c_read(uint8_t address, uint8_t *buffer, size_t len)
{
    return (i2c_read_blocking(i2c_default, address, buffer, len, true) == len);
}

bool i2c_write(uint8_t address, const uint8_t *buffer, size_t len)
{
    return (i2c_write_blocking(i2c_default, address, buffer, len, true) == len);
}

bool i2c_write_read(uint8_t address, const uint8_t *write_buffer, size_t write_len, uint8_t *read_buffer, size_t read_len)
{
    if (i2c_write_blocking(i2c_default, address, write_buffer, write_len, false) == write_len) {
        return (i2c_read_blocking(i2c_default, address, read_buffer, read_len, true) == read_len);
    }
    else {
        return false;
    }
}

#elif ESP32_SDK

void setup_i2c(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = 10,
        .scl_io_num = 9,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master = {
            .clk_speed = 400000,
        },
        .clk_flags = 0,
    };
    i2c_param_config(I2C_NUM_0, &conf);

    ESP_ERROR_CHECK(i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0));
}

bool i2c_read(uint8_t address, uint8_t *buffer, size_t len)
{
    return (i2c_master_read_from_device(I2C_NUM_0, address, buffer, len, 1000 / portTICK_PERIOD_MS) == ESP_OK);
}

bool i2c_write(uint8_t address, const uint8_t *buffer, size_t len)
{
    return (i2c_master_write_to_device(I2C_NUM_0, address, buffer, len, 1000 / portTICK_PERIOD_MS) == ESP_OK);
}

bool i2c_write_read(uint8_t address, const uint8_t *write_buffer, size_t write_len, uint8_t *read_buffer, size_t read_len)
{
    return (
        i2c_master_write_read_device(I2C_NUM_0, address, write_buffer, write_len,
            read_buffer, read_len, 1000 / portTICK_PERIOD_MS) == ESP_OK
    );
}

#endif
