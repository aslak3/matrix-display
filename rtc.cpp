#include <stdio.h>

#include <pico/stdlib.h>
#include <pico/cyw43_arch.h>

#include <hardware/i2c.h>

#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>

#include "messages.h"

#define PICO_I2C_SDA_PIN 20
#define PICO_I2C_SCL_PIN 21
#define RTC_I2C_ADDR 0x68

static void setup_rtc(void);
static void get_rtc_time(uint8_t *buffer);
static void get_rtc_temperature(uint8_t *buffer);
static void set_rtc_time(uint8_t *buffer);

extern QueueHandle_t rtc_queue;
extern QueueHandle_t animate_queue;

void rtc_task(void *dummy)
{
#if FREE_RTOS_KERNEL_SMP
    vTaskCoreAffinitySet(NULL, 1 << 0);
    printf("%s: core%u\n", pcTaskGetName(NULL), get_core_num());
#endif

    static message_t message = {
        message_type: MESSAGE_RTC,
    };
    TickType_t ticks_on_get_rtc_time = 0;

    setup_rtc();

    rtc_t old_rtc = {};

    while (1) {
        ticks_on_get_rtc_time = xTaskGetTickCount();
        get_rtc_time(message.rtc.datetime_buffer);

        // See if we are at the top of the next second
        if (memcmp(&message.rtc, &old_rtc, sizeof(rtc_t)) != 0) {
            get_rtc_temperature(message.rtc.temperature_buffer);

            if (xQueueSend(animate_queue, &message, 10) != pdTRUE) {
                printf("Could not send clock data; dropping");
            }
        }

        old_rtc = message.rtc;

        rtc_t set_rtc = {};

        if (xQueueReceive(rtc_queue, &set_rtc, 0) == pdTRUE) {
            printf("Clock set\n");
            set_rtc_time(set_rtc.datetime_buffer);
        }

        // Calculate a tenth of a second later since we got the time at the top of of the loop
        int tenth_second_delay = (((100 / portTICK_PERIOD_MS) + ticks_on_get_rtc_time)) - xTaskGetTickCount();
        
        if (tenth_second_delay > 0) {
            vTaskDelay(tenth_second_delay);
        }
    }
}

static void setup_rtc(void)
{
    // This example will use I2C0 on the default SDA and SCL pins (4, 5 on a Pico)
    i2c_init(i2c_default, 100 * 1000);
    gpio_set_function(PICO_I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(PICO_I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(PICO_I2C_SDA_PIN);
    gpio_pull_up(PICO_I2C_SCL_PIN);
}

static void get_rtc_time(uint8_t *buffer)
{
    uint8_t val = 0x00; // device address to read from

    // true to keep master control of bus
    i2c_write_blocking(i2c_default, RTC_I2C_ADDR, &val, 1, true);
    i2c_read_blocking(i2c_default, RTC_I2C_ADDR, buffer, RTC_DATETIME_LEN, false);
}

static void get_rtc_temperature(uint8_t *buffer)
{
    uint8_t val = 0x11; // device address to read from

    // true to keep master control of bus
    i2c_write_blocking(i2c_default, RTC_I2C_ADDR, &val, 1, true);
    i2c_read_blocking(i2c_default, RTC_I2C_ADDR, buffer, RTC_TEMPERATURE_LEN, false);
}

static void set_rtc_time(uint8_t *buffer)
{
    uint8_t out[RTC_DATETIME_LEN + 1];

    out[0] = 0x00;
    memcpy(&out[1], buffer, RTC_DATETIME_LEN);

    i2c_write_blocking(i2c_default, RTC_I2C_ADDR, out, 1 + RTC_DATETIME_LEN, false);
}