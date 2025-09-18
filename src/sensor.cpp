#include <stdio.h>

#include <pico/stdlib.h>
#include <pico/cyw43_arch.h>

#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>

#include "matrix_display.h"

#include "messages.h"

#include "i2c.h"

#define CLIMATE_SEND_INTERVAL 60    // Send climate data every 60 seconds

#if BME680_PRESENT
extern int configure_bme680(void);
extern int receive_data(void);
extern int request_run(void);

extern float get_temperature(void);
extern float get_pressure(void);
extern float get_humidity(void);
#elif DS3231_PRESENT
extern float get_temperature(void);
#endif

extern QueueHandle_t sensor_queue; // For listening
extern QueueHandle_t mqtt_queue;

void sensor_task(void *dummy)
{
    vTaskCoreAffinitySet(NULL, 1 << 0);
    DEBUG_printf("%s: core%u\n", pcTaskGetName(NULL), GET_CORE_NUMBER());
    
    setup_i2c();
    
    static message_mqtt_t message_mqtt = {
            message_type: MESSAGE_MQTT_CLIMATE,
    };
    
#if BME680_PRESENT
    bool got_data = false;
    bool requested_run = false;

    if (!(configure_bme680())) {
        DEBUG_printf("BME680: Configure failed\n");
    }
#endif

    static int climate_count = 0;

    while (1) {
        if (climate_count == CLIMATE_SEND_INTERVAL) {
#if BME680_PRESENT
            if (requested_run) {
                if (!(receive_data())) {
                    DEBUG_printf("BME680: Failed to recieve data\n");
                }
                got_data = true;
            }
            if (!(request_run())) {
                DEBUG_printf("BME680: Failed to request run\n");
            }
            requested_run = true;

            if (got_data) {
                message_mqtt.climate.temperature = get_temperature();
                message_mqtt.climate.pressure = get_pressure();
                message_mqtt.climate.humidity = get_humidity();

                if (xQueueSend(mqtt_queue, &message_mqtt, 10) != pdTRUE) {
                    DEBUG_printf("Could not send climate data; dropping\n");
                }
            }
#elif DS3231_PRESENT
            message_mqtt.climate.temperature = get_temperature();
            DEBUG_printf("Got temp from DS3231: %f\n", message_mqtt.climate.temperature);

            if (xQueueSend(mqtt_queue, &message_mqtt, 10) != pdTRUE) {
                DEBUG_printf("Could not send climate data; dropping\n");
            }

#endif

            climate_count = 0;
        }

        climate_count++;
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

