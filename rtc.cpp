#include <stdio.h>

#include <pico/stdlib.h>
#include <pico/cyw43_arch.h>

#include <hardware/i2c.h>

#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>

#include "messages.h"

#define PICO_I2C_SDA_PIN 16
#define PICO_I2C_SCL_PIN 17
#define RTC_I2C_ADDR 0x68

#define RTC_TEMPERATURE_LEN 2

static void setup_rtc(void);
static void get_rtc_time(uint8_t *buffer);
static float get_rtc_temperature(void);
static void set_rtc_time(uint8_t *buffer);

extern QueueHandle_t rtc_queue; // For listening
extern QueueHandle_t animate_queue;
extern QueueHandle_t mqtt_queue;

#define TEMPERATURE_BUFFER_SIZE 15

void rtc_task(void *dummy)
{
#if FREE_RTOS_KERNEL_SMP
    vTaskCoreAffinitySet(NULL, 1 << 0);
    printf("%s: core%u\n", pcTaskGetName(NULL), get_core_num());
#endif

    static message_anim_t message_anim = {
        message_type: MESSAGE_ANIM_RTC,
    };
    TickType_t ticks_on_get_rtc_time = 0;
    static message_mqtt_t message_mqtt = {
            message_type: MESSAGE_MQTT_TEMPERATURE,
    };
    setup_rtc();

    rtc_t old_rtc = {};
    static double circular_temperature_buffer[TEMPERATURE_BUFFER_SIZE];
    static int temperature_index = 0;
    static double old_average_temperature = 0.0;

    while (1) {
        ticks_on_get_rtc_time = xTaskGetTickCount();
        get_rtc_time(message_anim.rtc.datetime_buffer);

        // See if we are at the top of the next second
        if (memcmp(&message_anim.rtc, &old_rtc, sizeof(rtc_t)) != 0) {
            if (xQueueSend(animate_queue, &message_anim, 10) != pdTRUE) {
                printf("Could not send clock data; dropping");
            }

            circular_temperature_buffer[temperature_index++] = get_rtc_temperature();

            if (temperature_index == TEMPERATURE_BUFFER_SIZE) {
                temperature_index = 0;
                double current_average_temperature = 0;
                for (int i = 0; i < TEMPERATURE_BUFFER_SIZE; i++) {
                    current_average_temperature += circular_temperature_buffer[i];
                }
                current_average_temperature /= TEMPERATURE_BUFFER_SIZE;

                message_mqtt.temperature = current_average_temperature;
                if (xQueueSend(mqtt_queue, &message_mqtt, 10) != pdTRUE) {
                    printf("Could not send temperature data; dropping");
                }
            }
        }

        old_rtc = message_anim.rtc;

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

static float get_rtc_temperature(void)
{
    uint8_t buffer[RTC_TEMPERATURE_LEN];

    uint8_t val = 0x11; // device address to read from

    // true to keep master control of bus
    i2c_write_blocking(i2c_default, RTC_I2C_ADDR, &val, 1, true);
    i2c_read_blocking(i2c_default, RTC_I2C_ADDR, buffer, RTC_TEMPERATURE_LEN, false);

    return (float)(buffer[0]) + ((float)(buffer[1] >> 6) / 4.0);
}

static void set_rtc_time(uint8_t *buffer)
{
    uint8_t out[RTC_DATETIME_LEN + 1];

    out[0] = 0x00;
    memcpy(&out[1], buffer, RTC_DATETIME_LEN);

    i2c_write_blocking(i2c_default, RTC_I2C_ADDR, out, 1 + RTC_DATETIME_LEN, false);
}