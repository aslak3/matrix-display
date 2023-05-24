#include <stdio.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#include "hardware/i2c.h"

#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>

#define PICO_I2C_SDA_PIN 16
#define PICO_I2C_SCL_PIN 17

static void setup_rtc(void);
static void get_rtc_time(uint8_t *buffer, size_t length);

void rtc_task(void *dummy)
{
#if FREE_RTOS_KERNEL_SMP
    vTaskCoreAffinitySet(NULL, 1 << 0);
    printf("%s: core%u\n", pcTaskGetName(NULL), get_core_num());
#endif

    setup_rtc();

    while (1) {
        uint8_t buffer[7];

        get_rtc_time(buffer, 7);

        printf("RTC TIME: %02x:%02x:%02x\n",
            buffer[2] & 0x3f,
            buffer[1] & 0x7f,
            buffer[0] & 0x7f);
        
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void setup_rtc(void)
{
    i2c_init(i2c_default, 400 * 1000);

    // This example will use I2C0 on the default SDA and SCL pins (4, 5 on a Pico)
    i2c_init(i2c_default, 400 * 1000);
    gpio_set_function(PICO_I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(PICO_I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(PICO_I2C_SDA_PIN);
    gpio_pull_up(PICO_I2C_SCL_PIN);
}

void get_rtc_time(uint8_t *buffer, size_t length)
{
    uint8_t addr = 0x51;
    uint8_t val = 0x02; // device address to read from

    // true to keep master control of bus
    i2c_write_blocking(i2c_default, addr, &val, 1, true);
    i2c_read_blocking(i2c_default, addr, buffer, length, false);
}
