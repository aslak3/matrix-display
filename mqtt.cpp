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
static void handle_weather_data(char *attribute, char *data_as_chars);
static void handle_notificaiton_data(char *data_as_chars);
static void dump_weather_data(weather_data_t *weather_data);
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

        err = mqtt_subscribe(client, "matrix-display/#", 1, mqtt_sub_request_cb, arg);

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
    strncpy(current_topic, topic, 128);
}

extern QueueHandle_t animate_queue;

#define WEATHER_TOPIC "homeassistant/weather/openweathermap/"
#define NOTIFICATION_TOPIC "matrix-display/notification"

static void mqtt_incoming_data_cb(void *arg, const u8_t *data, u16_t len, u8_t flags)
{
    static char data_as_chars[16384];
    static char *p = data_as_chars;

    memcpy(p, data, len);
    p += len;
    *p = '\0';

    if (flags & MQTT_DATA_FLAG_LAST) {
        if (strncmp(current_topic, WEATHER_TOPIC, strlen(WEATHER_TOPIC)) == 0) {
            handle_weather_data(current_topic + strlen(WEATHER_TOPIC), data_as_chars);
        }
        else if (strcmp(current_topic, NOTIFICATION_TOPIC) == 0) {
            handle_notificaiton_data(data_as_chars);
        }
        else {
            printf("Unknown topic %s\n", current_topic);
        }
        memset(data_as_chars, 0, sizeof(data_as_chars));
        p = data_as_chars;
    }
}

static void handle_weather_data(char *attribute, char *data_as_chars)
{
    static weather_data_t weather_data;

    if (strcmp(attribute, "state") == 0) {
        strncpy(weather_data.condition, data_as_chars, CONDITION_LEN);
        weather_data.fetched_fields |= FIELD_CONDITION;
    }
    else {
        cJSON *json = cJSON_Parse(data_as_chars);

        if (json == NULL) {
            const char *error_ptr = cJSON_GetErrorPtr();
            if (error_ptr != NULL) {
                fprintf(stderr, "Error before: %s\n", error_ptr);
            }
        }
        else {
            if (cJSON_IsArray(json)) {
                if (strcmp(attribute, "forecast") == 0) {
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
                if (strcmp(attribute, "temperature") == 0) {
                    weather_data.temperature = value;
                    weather_data.fetched_fields |= FIELD_TEMPERATURE;
                }
                else if (strcmp(attribute, "humidity") == 0) {
                    weather_data.humidty = value;
                    weather_data.fetched_fields |= FIELD_HUMIDITY;
                }
                else if (strcmp(attribute, "wind_speed") == 0) {
                    weather_data.wind_speed = value;
                    weather_data.fetched_fields |= FIELD_WIND_SPEED;
                }
                else if (strcmp(attribute, "wind_bearing") == 0) {
                    weather_data.wind_bearing = value;
                    weather_data.fetched_fields |= FIELD_WIND_BEARING;
                }
                else if (strcmp(attribute, "pressure") == 0) {
                    weather_data.pressure = value;
                    weather_data.fetched_fields |= FIELD_PRESSURE;
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

    if (weather_data.fetched_fields == FIELD_ALL) {
        message_t message = {
            message_type: MESSAGE_WEATHER,
            weather_data: weather_data,
        };
        if (xQueueSend(animate_queue, &message, 10) != pdTRUE) {
            printf("Could not send weather data; dropping");
        }

        memset(&weather_data, 0, sizeof(weather_data_t));
    }
}

static void handle_notificaiton_data(char *data_as_chars)
{
    message_t message = {
        message_type: MESSAGE_NOTIFICATION,
    };

    strncpy(message.notification.text, data_as_chars, NOTIFICATION_LENGTH);

    if (xQueueSend(animate_queue, &message, 10) != pdTRUE) {
        printf("Could not send weather data; dropping");
    }
}
    
static void dump_weather_data(weather_data_t *weather_data)
{
    printf("--------------\n");
    printf("Current: condition=%s temperature=%.1f humidity=%.1f\n",
        weather_data->condition, weather_data->temperature, weather_data->humidty);
    for (int i = 0; i < weather_data->forecast_count; i++) {
        printf("%s: condition=%s temperature=%.1f percipitation_probability=%.1f\n",
            weather_data->forecast[i].time, weather_data->forecast[i].condition, 
            weather_data->forecast[i].temperature,
            weather_data->forecast[i].precipitation_probability);
    }
    printf("--------------\n");
}

static void mqtt_pub_request_cb(void *arg, err_t result)
{
    printf("Publish result: %d\n", result);
}
