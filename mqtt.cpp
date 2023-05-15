#include <stdio.h>

#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#include "mqtt_opts.h"

#include "lwip/apps/mqtt.h"

#include "mqtt.h"

#include "cJSON.h"

void led_task(void *dummy);

static int do_mqtt_connect(mqtt_client_t *client);

static void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status);
static void mqtt_sub_request_cb(void *arg, err_t result);
static void mqtt_pub_request_cb(void *arg, err_t result);
static void mqtt_incoming_publish_cb(void *arg, const char *topic, u32_t tot_len);
static void dump_weather_data(void);
static void mqtt_incoming_data_cb(void *arg, const u8_t *data, u16_t len, u8_t flags);

void led_task(void *dummy)
{
#if FREE_RTOS_KERNEL_SMP
    vTaskCoreAffinitySet(NULL, 1 << 0);
    printf("%s: core%u\n", pcTaskGetName(NULL), get_core_num());
#endif

    while (true) {
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
        vTaskDelay(1000);
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
        vTaskDelay(1000);
    }
}

void mqtt_task(void *dummy)
{
#if FREE_RTOS_KERNEL_SMP
    vTaskCoreAffinitySet(NULL, 1 << 0);
    printf("%s: core%u\n", pcTaskGetName(NULL), get_core_num());
#endif

    while (1) {
        if (cyw43_arch_init_with_country(CYW43_COUNTRY_UK)) {
            printf("failed to initialise\n");
            exit(1);
        }
        cyw43_arch_enable_sta_mode();

        printf("Connecting to WiFi...\n");
        // if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_MIXED_PSK, 300000)) {
        if (cyw43_arch_wifi_connect_blocking(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_MIXED_PSK)) {
            printf("failed to connect.\n");
            cyw43_arch_deinit();
        } else {
            printf("Connected.\n");
            break;
        }
    }

    xTaskCreate(&led_task, "LED Task", 256, NULL, 0, NULL);

    vTaskDelay(1000);

    mqtt_client_t *client = mqtt_client_new();

    /* Setup callback for incoming publish requests */
    mqtt_set_inpub_callback(client, mqtt_incoming_publish_cb, mqtt_incoming_data_cb, NULL);

    int err = do_mqtt_connect(client);

    /* For now just print the result code if something goes wrong */
    if (err != ERR_OK) {
        printf("do_mqtt_connect() return %d\n", err);
    }

    while (1) {
       vTaskDelay(1000);
    }
}

static int do_mqtt_connect(mqtt_client_t *client)
{
    struct mqtt_connect_client_info_t ci;

    /* Setup an empty client info structure */
    memset(&ci, 0, sizeof(ci));

    ci.client_id = "picow";
    ci.client_user = "mqttuser";
    ci.client_pass = "mqttpassword";
    ci.keep_alive = 60;

    ip_addr_t broker_addr;
    ip4addr_aton("10.52.0.2", &broker_addr);

    int err = mqtt_client_connect(client, &broker_addr, MQTT_PORT,
            mqtt_connection_cb, 0, &ci);

    return err;
}

static void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status)
{
    err_t err;
    if(status == MQTT_CONNECT_ACCEPTED) {
        printf("mqtt_connection_cb: Successfully connected\n");

        /* Subscribe to a topic named "topic" with QoS level 1, call mqtt_sub_request_cb with result */
        // living_room_temperature_sensor_temperature
        err = mqtt_subscribe(client, "homeassistant/weather/openweathermap/#", 1, mqtt_sub_request_cb, arg);

        if(err != ERR_OK) {
            printf("mqtt_subscribe return: %d\n", err);
        }
    } else {
        printf("Disconnected: %d\n", status);
        vTaskDelay(1000);
        do_mqtt_connect(client);
    }
}

static void mqtt_sub_request_cb(void *arg, err_t result)
{
    printf("Subscribe result: %d\n", result);
}

char current_topic[128];
static void mqtt_incoming_publish_cb(void *arg, const char *topic, u32_t tot_len)
{
    char *last_slash = strrchr(topic, '/');
    if (!last_slash) {
        panic("No last slash in topic");
    }

    strncpy(current_topic, last_slash + 1, 128);
}

extern char topic_message[256];
extern QueueHandle_t animate_queue;

char json_buffer[16384];
char *json_p = json_buffer;

weather_data_t weather_data;

static void mqtt_incoming_data_cb(void *arg, const u8_t *data, u16_t len, u8_t flags)
{
    memcpy(json_p, data, len);
    json_p += len;
    if (flags) {
        *json_p = '\0';
        json_p = json_buffer;

        if (strcmp(current_topic, "state") == 0) {
            strncpy(weather_data.condition, json_buffer, CONDITION_LEN);
            weather_data.fetched_fields |= FIELD_CONDITION;
        }
        else {
            cJSON *json = cJSON_Parse(json_buffer);

            if (json == NULL) {
                const char *error_ptr = cJSON_GetErrorPtr();
                if (error_ptr != NULL) {
                    fprintf(stderr, "Error before: %s\n", error_ptr);
                }
            }
            else
            {
                if (cJSON_IsArray(json)) {
                    if (strcmp(current_topic, "forecast") == 0) {
                        for (int i = 0; i < cJSON_GetArraySize(json) && i < NO_FORECASTS; i++) {
                            cJSON *item = cJSON_GetArrayItem(json, i);
                            cJSON *datetime = cJSON_GetObjectItem(item, "datetime");
                            cJSON *condition = cJSON_GetObjectItem(item, "condition");
                            cJSON *temperature = cJSON_GetObjectItem(item, "temperature");
                            cJSON *precipitation_probability = cJSON_GetObjectItem(item, "precipitation_probability");
                            if (!condition || !datetime || !temperature || !precipitation_probability) {
                                panic("Missing field(s)");
                            }

                            strncpy(weather_data.forecast[i].time, cJSON_GetStringValue(datetime) + 11, 2 + 1);
                            strncpy(weather_data.forecast[i].condition, cJSON_GetStringValue(condition), CONDITION_LEN);
                            weather_data.forecast[i].temperature = cJSON_GetNumberValue(temperature);
                            weather_data.forecast[i].precipitation_probability = cJSON_GetNumberValue(precipitation_probability);

                            weather_data.forecast_count = i + 1;
                        }
                        weather_data.fetched_fields |= FIELD_FORECAST;
                    }
                }
                else if (cJSON_IsString(json)) {
                }
                else if (cJSON_IsNumber(json)) {
                    double value = cJSON_GetNumberValue(json);
                    if (strcmp(current_topic, "temperature") == 0) {
                        weather_data.temperature = value;
                        weather_data.fetched_fields |= FIELD_TEMPERATURE;
                    }
                    else if (strcmp(current_topic, "humidity") == 0) {
                        weather_data.humidty = value;
                        weather_data.fetched_fields |= FIELD_HUMIDITY;
                    }
                }
                else if (cJSON_IsObject(json)) {
                    printf("Object\n");
                }
                else {
                    printf("Some other type\n");
                }

                cJSON_Delete(json);
            }
        }

        if (weather_data.fetched_fields == (FIELD_CONDITION | FIELD_TEMPERATURE | FIELD_HUMIDITY | FIELD_FORECAST)) {
            sprintf(topic_message, "%s %.1fC %.1f%% humidity; bye!!!", weather_data.condition, weather_data.temperature,
                weather_data.humidty);

            if (xQueueSend(animate_queue, &weather_data, 10) != pdTRUE) {
                printf("Could not send weather data; dropping");
            }

            memset(&weather_data, 0, sizeof(weather_data_t));
        }
    }
}

static void dump_weather_data(void)
{
    printf("--------------\n");
    printf("Current: condition=%s temperature=%.1f humidity=%.1f\n",
        weather_data.condition, weather_data.temperature, weather_data.humidty);
    for (int i = 0; i < weather_data.forecast_count; i++) {
        printf("%s: condition=%s temperature=%.1f percipitation_probability=%.1f\n",
            weather_data.forecast[i].time, weather_data.forecast[i].condition, 
            weather_data.forecast[i].temperature,
            weather_data.forecast[i].precipitation_probability);
    }
    printf("--------------\n");
}

static void mqtt_pub_request_cb(void *arg, err_t result)
{
    printf("Publish result: %d\n", result);
}
