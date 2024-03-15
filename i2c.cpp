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

#define DS3231_I2C_ADDR 0x68

#define CLIMATE_SEND_INTERVAL 10    // Send climate data every 10 seconds

#if BME680_PRESENT
extern int configure_bme680(void);
extern int receive_data(void);
extern int request_run(void);

extern float get_temperature(void);
extern float get_pressure(void);
extern float get_humidity(void);
#else
extern float get_temperature(void);
#endif

static void setup_i2c(void);
static void get_ds3231_time(uint8_t *buffer);
static void set_ds3231_time(uint8_t *buffer);

extern QueueHandle_t i2c_queue; // For listening
extern QueueHandle_t animate_queue;
extern QueueHandle_t mqtt_queue;

void i2c_task(void *dummy)
{
#if FREE_RTOS_KERNEL_SMP
    vTaskCoreAffinitySet(NULL, 1 << 0);
    printf("%s: core%u\n", pcTaskGetName(NULL), get_core_num());
#endif

    static message_anim_t message_anim = {
        message_type: MESSAGE_ANIM_DS3231,
    };

    TickType_t ticks_on_get_ds3231_time = 0;

    static message_mqtt_t message_mqtt = {
            message_type: MESSAGE_MQTT_CLIMATE,
    };
    setup_i2c();

    ds3231_t old_ds3231 = {};

#if BME680_PRESENT
    bool got_data = false;
    bool requested_run = false;

    if (!(configure_bme680())) {
        printf("BME680: Configure failed\n");
    }
#endif

    static int climate_count = 0;

    while (1) {
        ticks_on_get_ds3231_time = xTaskGetTickCount();
        get_ds3231_time(message_anim.ds3231.datetime_buffer);

        // See if we are at the top of the next second
        if (memcmp(&message_anim.ds3231, &old_ds3231, sizeof(ds3231_t)) != 0) {
            if (xQueueSend(animate_queue, &message_anim, 10) != pdTRUE) {
                printf("Could not send clock data; dropping");
            }

            if (climate_count == CLIMATE_SEND_INTERVAL) {
#if BME680_PRESENT
                if (requested_run) {
                    if (!(receive_data())) {
                        printf("BME680: Failed to recieve data\n");
                    }
                    got_data = true;
                }
                if (!(request_run())) {
                    printf("BME680: Failed to request run\n");
                }
                requested_run = true;

                if (got_data) {
                    message_mqtt.climate.temperature = get_temperature();
                    message_mqtt.climate.pressure = get_pressure();
                    message_mqtt.climate.humidity = get_humidity();

                    if (xQueueSend(mqtt_queue, &message_mqtt, 10) != pdTRUE) {
                        printf("Could not send climate data; dropping");
                    }
                }
#else
                message_mqtt.climate.temperature = get_temperature();

                if (xQueueSend(mqtt_queue, &message_mqtt, 10) != pdTRUE) {
                    printf("Could not send climate data; dropping");
                }

#endif

                climate_count = 0;
            }

            climate_count++;
        }

        old_ds3231 = message_anim.ds3231;

        ds3231_t set_ds3231 = {};

        if (xQueueReceive(i2c_queue, &set_ds3231, 0) == pdTRUE) {
            printf("Clock set\n");
            set_ds3231_time(set_ds3231.datetime_buffer);
        }

        // Calculate a tenth of a second later since we got the time at the top of of the loop
        int tenth_second_delay = (((100 / portTICK_PERIOD_MS) + ticks_on_get_ds3231_time)) - xTaskGetTickCount();

        if (tenth_second_delay > 0) {
            vTaskDelay(tenth_second_delay);
        }
    }
}

static void setup_i2c(void)
{
    // This example will use I2C0 on the default SDA and SCL pins (4, 5 on a Pico)
    i2c_init(i2c_default, 100 * 1000);
    gpio_set_function(PICO_I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(PICO_I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(PICO_I2C_SDA_PIN);
    gpio_pull_up(PICO_I2C_SCL_PIN);
}

static void get_ds3231_time(uint8_t *buffer)
{
    uint8_t val = 0x00; // device address to read from

    // true to keep master control of bus
    i2c_write_blocking(i2c_default, DS3231_I2C_ADDR, &val, 1, true);
    i2c_read_blocking(i2c_default, DS3231_I2C_ADDR, buffer, DS3231_DATETIME_LEN, false);
}

static void set_ds3231_time(uint8_t *buffer)
{
    uint8_t out[DS3231_DATETIME_LEN + 1];

    out[0] = 0x00;
    memcpy(&out[1], buffer, DS3231_DATETIME_LEN);

    i2c_write_blocking(i2c_default, DS3231_I2C_ADDR, out, 1 + DS3231_DATETIME_LEN, false);
}
