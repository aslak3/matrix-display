#include <stdio.h>
#include <stdlib.h>

#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>

#include <pico/stdlib.h>
#include <pico/cyw43_arch.h>

#include "mqtt_opts.h"
#include <lwip/apps/mqtt.h>

#include "messages.h"

#include "cJSON.h"

extern QueueHandle_t animate_queue;
extern QueueHandle_t rtc_queue;

void led_task(void *dummy);

static void connect_wifi(void);
static int do_mqtt_connect(mqtt_client_t *client);
static void do_mqtt_subscribe(mqtt_client_t *client);
static void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status);
static void mqtt_sub_request_cb(void *arg, err_t result);
static void mqtt_pub_request_cb(void *arg, err_t result);
static void mqtt_incoming_publish_cb(void *arg, const char *topic, u32_t tot_len);

static void handle_weather_data(char *attribute, char *data_as_chars);
static void handle_media_player_data(char *attribute, char *data_as_chars);
static void handle_calendar_data(char *attribute, char *data_as_chars);
static void handle_bluestar_data(char *data_as_chars);
static void handle_porch_sensor_data(char *data_as_chars);
static void handle_notificaiton_data(char *data_as_chars);
static void handle_set_rtc_time_data(char *data_as_chars);
static void handle_set_brightness_data(char *attribute, char *data_as_chars);
static void handle_set_grayscale_data(char *data_as_chars);
static void dump_weather_data(weather_data_t *weather_data);
static void mqtt_incoming_data_cb(void *arg, const u8_t *data, u16_t len, u8_t flags);

static uint8_t bcd_digit_to_byte(char c);
static char *bcd_two_digits_to_byte(char *in, uint8_t *out);
static void bcd_string_to_bytes(char *in, uint8_t *buffer, uint8_t len);

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

    xTaskCreate(&led_task, "LED Task", 256, NULL, 0, NULL);

    while (1) {
        if (cyw43_arch_init_with_country(CYW43_COUNTRY_UK)) {
            printf("failed to initialise\n");
            exit(1);
        }
        cyw43_arch_enable_sta_mode();

        printf("Connecting to WiFi...\n");
        if (cyw43_arch_wifi_connect_blocking(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_MIXED_PSK)) {
            printf("failed to connect.\n");
            cyw43_arch_deinit();
        } else {
            printf("Connected.\n");
            break;
        }
    }

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

    cyw43_arch_lwip_begin();

    int err = mqtt_client_connect(client, &broker_addr, MQTT_PORT,
        mqtt_connection_cb, 0, &ci);

    cyw43_arch_lwip_end();

    return err;
}

static void do_mqtt_subscribe(mqtt_client_t *client)
{
    const char *subscriptions[] = {
        "homeassistant/weather/openweathermap/#",
        "homeassistant/media_player/squeezebox/state",
        "homeassistant/media_player/squeezebox/media_title",
        "homeassistant/media_player/squeezebox/media_artist",
        "homeassistant/media_player/squeezebox/media_album_name",
        "homeassistant/binary_sensor/porch_motion_sensor_iaszone/state",
        // "homeassistant/climate/hallway/temperature",
        // "homeassistant/sensor/living_room_temperature_sensor_temperature/state",
        // "homeassistant/sensor/porch_temperature/state",
        // "homeassistant/sensor/lumi_lumi_weather_temperature/state",
        "matrix_display/#",
        "bluestar_parser/#",
        NULL,
    };

    void *arg = NULL;
    int err;

    for (const char **s = subscriptions; *s; s++) {
        err = mqtt_subscribe(client, *s, 1, mqtt_sub_request_cb, arg);
        if(err != ERR_OK) {
            printf("mqtt_subscribe on %s return: %d\n", *s, err);
        }
        vTaskDelay(10);
    }

    const char *sub_topics[] = { "start", "summary", NULL };
    for (int a = 0; a < NO_APPOINTMENTS; a++) {
        for (const char **st = sub_topics; **st; st++) {
            char subscription[64];

            snprintf(subscription, sizeof(subscription), "homeassistant/sensor/ical_our_house_event_%d/%s", a, *st);
            err = mqtt_subscribe(client, subscription, 1, mqtt_sub_request_cb, arg);
            if (err != ERR_OK) {
                printf("mqtt_subscribe on %s return: %d\n", subscription, err);
            }
            vTaskDelay(10);
        }
    }
}

static void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status)
{
    if (status == MQTT_CONNECT_ACCEPTED) {
        printf("Connected\n");
        do_mqtt_subscribe(client);
    }
    else {
        printf("Disconnected: %d\n", status);
        vTaskDelay(1000);
        printf("Reconnecting MQTT\n");
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
    strncpy(current_topic, topic, sizeof(current_topic) - 1);
}

// extern QueueHandle_t animate_queue;

#define WEATHER_TOPIC "homeassistant/weather/openweathermap/"
#define MEDIA_PLAYER_TOPIC "homeassistant/media_player/squeezebox/"
#define CALENDAR_TOPIC "homeassistant/sensor/ical_our_house_event_"
#define BUS_JOURNIES "bluestar_parser/journies"
#define PORCH_SENSOR_TOPIC "homeassistant/binary_sensor/porch_motion_sensor_iaszone/state"
#define NOTIFICATION_TOPIC "matrix_display/notification"
#define SET_RTC_TIME_TOPIC "matrix_display/set_rtc_time"
// Includes brightness and brightness_red etc
#define SET_BRIGHTNESS_TOPIC "matrix_display/brightness_"
#define SET_GRAYSCALE_TOPIC "matrix_display/grayscale"

static void mqtt_incoming_data_cb(void *arg, const u8_t *data, u16_t len, u8_t flags)
{
    // printf("Start of mqtt_incoming_data_cb(flags: %d)\n", flags);
    static char data_as_chars[16384];
    static char *p = data_as_chars;

    memcpy(p, data, len);
    p += len;
    *p = '\0';

    if (p - data_as_chars > 16384 - 1500) {
        panic("mqtt_incoming_data_cb(): data is too long");
    }

    if (flags & MQTT_DATA_FLAG_LAST) {
        if (strncmp(current_topic, WEATHER_TOPIC, strlen(WEATHER_TOPIC)) == 0) {
            handle_weather_data(current_topic + strlen(WEATHER_TOPIC), data_as_chars);
        }
        else if (strncmp(current_topic, MEDIA_PLAYER_TOPIC, strlen(MEDIA_PLAYER_TOPIC)) == 0) {
            handle_media_player_data(current_topic + strlen(MEDIA_PLAYER_TOPIC), data_as_chars);
        }
        else if (strncmp(current_topic, CALENDAR_TOPIC, strlen(CALENDAR_TOPIC)) == 0) {
            handle_calendar_data(current_topic + strlen(CALENDAR_TOPIC), data_as_chars);
        }
        else if (strcmp(current_topic, BUS_JOURNIES) == 0) {
            handle_bluestar_data(data_as_chars);
        }
        else if( strcmp(current_topic, PORCH_SENSOR_TOPIC) == 0) {
            handle_porch_sensor_data(data_as_chars);
        }
        else if (strcmp(current_topic, NOTIFICATION_TOPIC) == 0) {
            handle_notificaiton_data(data_as_chars);
        }
        else if (strcmp(current_topic, SET_RTC_TIME_TOPIC) == 0) {
            handle_set_rtc_time_data(data_as_chars);
        }
        else if (strncmp(current_topic, SET_BRIGHTNESS_TOPIC, strlen(SET_BRIGHTNESS_TOPIC)) == 0) {
            handle_set_brightness_data(current_topic + strlen(SET_BRIGHTNESS_TOPIC), data_as_chars);
        }
        else if (strcmp(current_topic, SET_GRAYSCALE_TOPIC) == 0) {
            handle_set_grayscale_data(data_as_chars);
        }
        else {
            printf("Unknown topic %s\n", current_topic);
        }
        memset(data_as_chars, 0, sizeof(data_as_chars));
        p = data_as_chars;
    }
    // printf("Done incoming_data_cb()\n");
}

static message_t message;

static void handle_weather_data(char *attribute, char *data_as_chars)
{
    printf("Weather update\n");

    static weather_data_t weather_data;

    if (strcmp(attribute, "state") == 0) {
        strncpy(weather_data.condition, data_as_chars, sizeof(weather_data.condition));
        weather_data.fetched_fields |= FIELD_WD_CONDITION;
    }
    else {
        cJSON *json = cJSON_Parse(data_as_chars);

        if (json == NULL) {
            const char *error_ptr = cJSON_GetErrorPtr();
            if (error_ptr != NULL) {
                fprintf(stderr, "Error before: %s\n", error_ptr);
            }
            else {
                fprintf(stderr, "Unknown JSON parse error\n");
            }
            return;
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
                        strncpy(weather_data.forecast[i].condition, cJSON_GetStringValue(condition),
                            sizeof(weather_data.forecast[i].condition));
                        weather_data.forecast[i].temperature = cJSON_GetNumberValue(temperature);
                        weather_data.forecast[i].precipitation_probability = cJSON_GetNumberValue(precipitation_probability);

                        weather_data.forecast_count = i + 1;
                    }
                    weather_data.fetched_fields |= FIELD_WD_FORECAST;
                }
            }
            else if (cJSON_IsString(json)) {
            }
            else if (cJSON_IsNumber(json)) {
                double value = cJSON_GetNumberValue(json);
                if (strcmp(attribute, "temperature") == 0) {
                    weather_data.temperature = value;
                    weather_data.fetched_fields |= FIELD_WD_TEMPERATURE;
                }
                else if (strcmp(attribute, "humidity") == 0) {
                    weather_data.humidty = value;
                    weather_data.fetched_fields |= FIELD_WD_HUMIDITY;
                }
                else if (strcmp(attribute, "wind_speed") == 0) {
                    weather_data.wind_speed = value;
                    weather_data.fetched_fields |= FIELD_WD_WIND_SPEED;
                }
                else if (strcmp(attribute, "wind_bearing") == 0) {
                    weather_data.wind_bearing = value;
                    weather_data.fetched_fields |= FIELD_WD_WIND_BEARING;
                }
                else if (strcmp(attribute, "pressure") == 0) {
                    weather_data.pressure = value;
                    weather_data.fetched_fields |= FIELD_WD_PRESSURE;
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

    if (weather_data.fetched_fields == FIELD_WD_ALL) {
        message = {
            message_type: MESSAGE_WEATHER,
            weather_data: weather_data,
        };
        if (xQueueSend(animate_queue, &message, 10) != pdTRUE) {
            printf("Could not send weather data; dropping\n");
        }

        memset(&weather_data, 0, sizeof(weather_data_t));
    }
}

static void handle_media_player_data(char *attribute, char *data_as_chars)
{
    printf("Media player update\n");

    static media_player_data_t media_player_data;

    if (!(
        (strcmp(attribute, "state") == 0) ||
        (strcmp(attribute, "media_title") == 0) ||
        (strcmp(attribute, "media_artist") == 0) ||
        (strcmp(attribute, "media_album_name") == 0)
    )) {
        // printf("Nothing to do\n");
        return;
    }

    bool send_update = false;

    if (strcmp(attribute, "state") == 0) {
        strncpy(media_player_data.state, data_as_chars, sizeof(media_player_data.state));
        send_update = true;
    }
    else {
        // printf("attribute: %s, value: %s\n", attribute, data_as_chars);
        cJSON *json = cJSON_Parse(data_as_chars);

        if (json == NULL) {
            const char *error_ptr = cJSON_GetErrorPtr();
            if (error_ptr != NULL) {
                fprintf(stderr, "Error before: %s\n", error_ptr);
            }
            else {
                fprintf(stderr, "Unknown JSON parse error\n");
            }
            return;
        }

        if (cJSON_IsString(json)) {
            char *string = cJSON_GetStringValue(json);
            if (strcmp(attribute, "media_title") == 0) {
                strncpy(media_player_data.media_title, cJSON_GetStringValue(json),
                    sizeof(media_player_data.media_title));
            }
            else if (strcmp(attribute, "media_artist") == 0) {
                strncpy(media_player_data.media_artist, cJSON_GetStringValue(json),
                    sizeof(media_player_data.media_artist));
            }
            else if (strcmp(attribute, "media_album_name") == 0) {
                strncpy(media_player_data.media_album_name,  cJSON_GetStringValue(json),
                    sizeof(media_player_data.media_album_name));
                send_update = true;
            }
        }

        cJSON_Delete(json);
    }

    if (send_update)  {
        message = {
            message_type: MESSAGE_MEDIA_PLAYER,
            media_player_data: media_player_data,
        };
        if (xQueueSend(animate_queue, &message, 10) != pdTRUE) {
            printf("Could not send media_player data; dropping\n");
        }
    }
}

static void handle_calendar_data(char *attribute, char *data_as_chars)
{
    printf("Calendar update\n");

    static calendar_data_t calendar_data;

    attribute[1] = '\0';

    int appointment_index = atol(attribute);

    if (!(appointment_index >= 0 && appointment_index <= NO_APPOINTMENTS)) {
        panic("Can only deal with %d appointments, got %d\n", NO_APPOINTMENTS, appointment_index);
    }

    cJSON *json = cJSON_Parse(data_as_chars);

    if (json == NULL) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL) {
            fprintf(stderr, "Error before: %s\n", error_ptr);
        }
        else {
            fprintf(stderr, "Unknown JSON parse error\n");
        }
        return;
    }

    appointment_t *app = &calendar_data.appointments[appointment_index];

    if (cJSON_IsString(json)) {
        char *string = cJSON_GetStringValue(json);
        if (strcmp(&attribute[2], "start") == 0) {
            const char *value = cJSON_GetStringValue(json);
            if (strlen(value) < 25) {
                panic("Start datetime is badly formed: %s", value);
            }
            else {
                strncpy(app->start, &value[5], 5);
                app->start[5] = '\0';
                strncat(app->start, " ", 2);
                strncat(app->start, &value[11], 5);
            }
        }
        else if (strcmp(&attribute[2], "summary") == 0) {
            strncpy(app->summary, cJSON_GetStringValue(json), sizeof(app->summary));
        }
    }

    cJSON_Delete(json);

    message = {
        message_type: MESSAGE_CALENDAR,
        calendar_data: calendar_data,
    };
    // printf("Sending calendar data\n");
    if (xQueueSend(animate_queue, &message, 10) != pdTRUE) {
        printf("Could not send calendar data; dropping\n");
    }

    // printf("handle_calendar_data attribute %s data %s\n", attribute, data_as_chars);
}

static void handle_bluestar_data(char *data_as_chars)
{
    printf("Bluestar update %s\n", data_as_chars);

    static bluestar_data_t bluestar_data;

    cJSON *json = cJSON_Parse(data_as_chars);

    if (json == NULL) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL) {
            printf("Error before: %s\n", error_ptr);
        }
        else {
            printf("Unknown JSON parse error\n");
        }
        return;
    }

    if (cJSON_IsArray(json)) {
        for (int i = 0; i < cJSON_GetArraySize(json) && i < NO_BUS_JOURNIES; i++) {
            cJSON *item = cJSON_GetArrayItem(json, i);
            cJSON *towards = cJSON_GetObjectItem(item, "Towards");
            cJSON *departures_summary = cJSON_GetObjectItem(item, "DeparturesSummary");

            if (!towards || !departures_summary) {
                printf("Missing field(s)");
            }

            strncpy(bluestar_data.journies[i].towards, cJSON_GetStringValue(towards), 16);
            strncpy(bluestar_data.journies[i].departures_summary, cJSON_GetStringValue(departures_summary), 64);
        }

        message_t message = {
            message_type: MESSAGE_BLUESTAR,
            bluestar_data: bluestar_data,
        };
        printf("Sending bluestar data\n");
        if (xQueueSend(animate_queue, &message, 10) != pdTRUE) {
            printf("Could not send bluestar data; dropping\n");
        }
    }
    else {
        printf("Not an arrray in bluestar data\n");
    }

    cJSON_Delete(json);
}

static void handle_porch_sensor_data(char *data_as_chars)
{
    printf("Porch update: %s\n", data_as_chars);

    porch_t porch = {
        occupied: (strcmp(data_as_chars, "on") == 0) ? true : false,
    };

    message = {
        message_type: MESSAGE_PORCH,
        porch: porch,
    };
    if (xQueueSend(animate_queue, &message, 10) != pdTRUE) {
        printf("Could not send media_player data; dropping");
    }
}

static void handle_notificaiton_data(char *data_as_chars)
{
    printf("Notifation update: %s", data_as_chars);

    message = {
        message_type: MESSAGE_NOTIFICATION,
    };

    strncpy(message.notification.text, data_as_chars, sizeof(message.notification.text));

    if (xQueueSend(animate_queue, &message, 10) != pdTRUE) {
        printf("Could not send weather data; dropping");
    }
}

static void handle_set_rtc_time_data(char *data_as_chars)
{
    printf("handle_set_rtc_time_data()\n");

    rtc_t rtc;

    bcd_string_to_bytes(data_as_chars, rtc.datetime_buffer, RTC_DATETIME_LEN);

    if (xQueueSend(rtc_queue, &rtc, 10) != pdTRUE) {
        printf("Could not send rtc data; dropping");
    }
}

static void handle_set_brightness_data(char *attribute, char *data_as_chars)
{
    printf("Brightness update: %s is %s\n", attribute, data_as_chars);

    brightness_t brightness;

    if (strcmp(attribute, "red") == 0) {
        brightness.type = BRIGHTNESS_RED;
    } else if (strcmp(attribute, "green") == 0) {
        brightness.type = BRIGHTNESS_GREEN;
    } else if (strcmp(attribute, "blue") == 0) {
        brightness.type = BRIGHTNESS_BLUE;
    } else {
        brightness.type = BRIGHTNESS_UNKNWON;
        printf("Malformed brightness type [%s]\n");
    }
    brightness.intensity = atol(data_as_chars);

    message = {
        message_type: MESSAGE_BRIGHTNESS,
        brightness: brightness,
    };

    if (xQueueSend(animate_queue, &message, 10) != pdTRUE) {
        printf("Could not send brightness data; dropping");
    }
}

static void handle_set_grayscale_data(char *data_as_chars)
{
    printf("Grayscale update: %s", data_as_chars);

    message = {
        message_type: MESSAGE_GRAYSCALE,
    };

    if (strcmp(data_as_chars, "ON") == 0) {
        message.grayscale = true;
    } else if (strcmp(data_as_chars, "OFF") == 0) {
        message.grayscale = false;
    } else {
        message.grayscale = false;
        printf("Malformed grayscale %s\n", data_as_chars);
    }

    if (xQueueSend(animate_queue, &message, 10) != pdTRUE) {
        printf("Could not send grayscale data; dropping");
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

static uint8_t bcd_digit_to_byte(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    } else if (c >= 'a' && c <= 'f') {
        return c - 'a';
    }
    return 0;
}

static char *bcd_two_digits_to_byte(char *in, uint8_t *out)
{
    char *c = in;
    uint8_t result = 0;

    if (*c) {
        result |= bcd_digit_to_byte(*c);
        c++;
        if (*c) {
            result <<= 4;
            result |= bcd_digit_to_byte(*c);
            c++;
        }
        else {
            return NULL;
        }
    }
    else {
        return NULL;
    }

    *out = result;

    return c;
}

static void bcd_string_to_bytes(char *in, uint8_t *buffer, uint8_t len)
{
    uint8_t one_byte = 0;
    uint8_t *out = buffer;

    char *c = in;
    while (*c) {
        c = bcd_two_digits_to_byte(c, &one_byte);
        if (!c) {
            break;
        }
        if (out - buffer > len) {
            break;
        }
        *out = one_byte;
        out++;
    }
}