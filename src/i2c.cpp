#include <stdlib.h>

#if PICO_SDK
#include <pico/stdlib.h>
#include <pico/cyw43_arch.h>

#include <hardware/i2c.h>
#elif ESP32_SDK

#endif

#if PICO_SDK

#define PICO_I2C_SDA_PIN 20
#define PICO_I2C_SCL_PIN 21

// https://embeddedexplorer.com/esp32-i2c-tutorial/

void setup_i2c(void)
{
    // This example will use I2C0 on the default SDA and SCL pins (4, 5 on a Pico)
    i2c_init(i2c_default, 100 * 1000);
    gpio_set_function(PICO_I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(PICO_I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(PICO_I2C_SDA_PIN);
    gpio_pull_up(PICO_I2C_SCL_PIN);
}

int i2c_read(uint8_t address, uint8_t *buffer, size_t len, bool nostop)
{
    return i2c_read_blocking(i2c_default, address, buffer, len, nostop);
}

int i2c_write(uint8_t address, const uint8_t *buffer, size_t len, bool nostop)
{
    return i2c_write_blocking(i2c_default, address, buffer, len, nostop);
}

#elif ESP32_SDK

#endif
