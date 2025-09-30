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
#include "messages.h"
#include "sensor.h"
#include "matrix_display.h"

#define SENSOR_SEND_INTERVAL 60    // Send sensor data every 60 seconds (approx)

extern QueueHandle_t sensor_queue; // For listening
extern QueueHandle_t mqtt_queue;

Sensor *avail_sensors[] = {
#if DS3231_PRESENT
    DS3231Sensor::create("DS3231"),
#endif
#if BME680_PRESENT
    BME680Sensor::create("BME680"),
#endif
#if BH1750_PRESENT
    BH1750Sensor::create("BH1750"),
#endif
NULL,
};

void sensor_task(void *dummy)
{
    vTaskCoreAffinitySet(NULL, 1 << 0);
    DEBUG_printf("%s: core%u\n", pcTaskGetName(NULL), GET_CORE_NUMBER());

    setup_i2c();

    for (int sensor_count = 0; avail_sensors[sensor_count]; sensor_count++) {
        const char *name = avail_sensors[sensor_count]->get_name();
        DEBUG_printf(
            "Configuring %s sensor, if enabled\n", name);
        if (!avail_sensors[sensor_count]->configure()) {
            DEBUG_printf("Could not configure sensor %s\n", name);
        }
    }

    static int period_count = 0;

    while (1) {
        if (period_count == SENSOR_SEND_INTERVAL - 1) {
            for (int sensor_count = 0; avail_sensors[sensor_count]; sensor_count++) {
                if (!avail_sensors[sensor_count]->request_run()) {
                    DEBUG_printf(
                        "Could not request a run on sensor %s\n",
                        avail_sensors[sensor_count]->get_name()
                    );
                }
            }
        }

        static MessageMQTT_t message_mqtt;
        message_mqtt.message_type = MESSAGE_MQTT_SENSOR;

        if (period_count == SENSOR_SEND_INTERVAL) {
            for (int sensor_count = 0; avail_sensors[sensor_count]; sensor_count++) {
                Sensor *sensor = avail_sensors[sensor_count];

                if (!sensor->receive_data()) {
                    DEBUG_printf("Could not receive data on sensor %s\n", sensor->get_name());
                    continue;
                }

                if (sensor->temperature_sensor()) {
                    message_mqtt.sensor.temperature = sensor->temperature_sensor()->get_temperature();
                    DEBUG_printf(
                        "Got temperature %f from sensor %s\n",
                        message_mqtt.sensor.temperature, sensor->get_name()
                    );
                }

                if (sensor->humidity_sensor()) {
                    message_mqtt.sensor.humidity = sensor->humidity_sensor()->get_humidity();
                    DEBUG_printf(
                        "Got humidity %f from sensor %s\n",
                        message_mqtt.sensor.humidity, sensor->get_name()
                    );
                }

                if (sensor->pressure_sensor()) {
                    message_mqtt.sensor.pressure = sensor->pressure_sensor()->get_pressure();
                    DEBUG_printf(
                        "Got pressure %f from sensor %s\n",
                        message_mqtt.sensor.pressure, sensor->get_name()
                    );
                }

                if (sensor->illuminance_sensor()) {
                    message_mqtt.sensor.illuminance = sensor->illuminance_sensor()->get_illuminance();
                    DEBUG_printf(
                        "Got illuminance %u from sensor %s\n",
                        message_mqtt.sensor.illuminance, sensor->get_name()
                    );
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

