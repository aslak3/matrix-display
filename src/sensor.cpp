#include <stdio.h>
#include <string.h>

#if PICO_SDK
#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>

#include <pico/stdlib.h>
#elif ESP32_SDK
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#endif

#include "i2c.h"

#include "matrix_display.h"
#include "messages.h"
#include "sensor.h"

#define SENSOR_SEND_INTERVAL 60    // Send sensor data every 60 seconds (approx)

extern QueueHandle_t sensor_queue; // For listening
extern QueueHandle_t mqtt_queue;

Sensor *avail_sensors[] = {
#if DS3231_PRESENT
    DS3231Sensor::create(),
#endif
#if BME680_PRESENT
    BME680Sensor::create(),
#endif
#if BH1750_PRESENT
    BH1750Sensor::create(),
#endif
NULL,
};

void sensor_task(void *dummy)
{
    vTaskCoreAffinitySet(NULL, 1 << 0);
    DEBUG_printf("%s: core%u\n", pcTaskGetName(NULL), GET_CORE_NUMBER());

    setup_i2c();

    static message_mqtt_t message_mqtt = {
            message_type: MESSAGE_MQTT_SENSOR,
    };

    for (int sensor_count = 0; avail_sensors[sensor_count]; sensor_count++) {
        avail_sensors[sensor_count]->configure();
    }

    static int period_count = 0;

    while (1) {
        if (period_count == SENSOR_SEND_INTERVAL - 1) {
            for (int sensor_count = 0; avail_sensors[sensor_count]; sensor_count++) {
                avail_sensors[sensor_count]->request_run();
            }
        }

        if (period_count == SENSOR_SEND_INTERVAL) {
            for (int sensor_count = 0; avail_sensors[sensor_count]; sensor_count++) {
                avail_sensors[sensor_count]->receive_data();

                if (avail_sensors[sensor_count]->temperature_sensor()) {
                    message_mqtt.sensor.temperature =
                        avail_sensors[sensor_count]->temperature_sensor()->get_temperature();
                    DEBUG_printf("Got temperature: %f\n", message_mqtt.sensor.temperature);
                }

                if (avail_sensors[sensor_count]->humidity_sensor()) {
                    message_mqtt.sensor.humidity =
                        avail_sensors[sensor_count]->humidity_sensor()->get_humidity();
                    DEBUG_printf("Got humidity: %f\n", message_mqtt.sensor.humidity);
                }

                if (avail_sensors[sensor_count]->pressure_sensor()) {
                    message_mqtt.sensor.pressure =
                        avail_sensors[sensor_count]->pressure_sensor()->get_pressure();
                    DEBUG_printf("Got pressure: %f\n", message_mqtt.sensor.pressure);
                }

                if (avail_sensors[sensor_count]->illuminance_sensor()) {
                    message_mqtt.sensor.illuminance =
                        avail_sensors[sensor_count]->illuminance_sensor()->get_illuminance();
                    DEBUG_printf("Got illuminance: %u\n", message_mqtt.sensor.illuminance);
                }
            }

            if (xQueueSend(mqtt_queue, &message_mqtt, 10) != pdTRUE) {
                DEBUG_printf("Could not send sensor data; dropping\n");
            }

            period_count = 0;
        }

        period_count++;
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

