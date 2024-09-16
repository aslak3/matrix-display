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

extern QueueHandle_t mqtt_queue;
extern QueueHandle_t animate_queue;
extern QueueHandle_t i2c_queue;
extern QueueHandle_t buzzer_queue;

void led_task(void *dummy);

static void connect_wifi(void);
static int do_mqtt_connect(mqtt_client_t *client);
static void do_mqtt_subscribe(mqtt_client_t *client);
static void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status);
static void mqtt_sub_request_cb(void *arg, err_t result);
static void mqtt_pub_request_cb(void *arg, err_t result);
static void mqtt_incoming_publish_cb(void *arg, const char *topic, u32_t tot_len);
static cJSON *json_parser(char *data_as_chars);

static void handle_weather_json_data(char *data_as_chars);
static void handle_media_player_json_data(char *data_as_chars);
static void handle_calendar_data(char *data_as_chars);
static void handle_scroller_json_data(char *data_as_chars);
static void handle_transport_json_data(char *data_as_chars);
static void handle_porch_sensor_data(char *data_as_chars);
static void handle_notificaiton_data(char *data_as_chars);
static void handle_set_time_data(char *data_as_chars);
static void handle_buzzer_play_data(char *data_as_chars);
static void handle_light_command_data(char *data_as_chars);
static void handle_light_brightness_command_data(char *data_as_chars);
static void handle_set_grayscale_data(char *data_as_chars);
static void handle_set_snowflakes_data(char *data_as_chars);
static void handle_configuration_data(char *attribute, char *data_as_chars);
static void handle_autodiscover_control_data(char *data_as_chars);

static void dump_weather_data(weather_data_t *weather_data);

cJSON *create_base_object(const char *name, const char *unique_id);
int publish_object_as_device_entity(cJSON *obj, cJSON *device, mqtt_client_t *client, const char *topic);
static void publish_loop_body(mqtt_client_t *client);
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

#define TEMPERATURE_TOPIC "matrix_display/temperature"
#if BME680_PRESENT
#define PRESSURE_TOPIC "matrix_display/pressure"
#define HUMIDITY_TOPIC "matrix_display/humidity"
#endif

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
        publish_loop_body(client);
    }
}

static int do_mqtt_connect(mqtt_client_t *client)
{
    struct mqtt_connect_client_info_t ci;

    /* Setup an empty client info structure */
    memset(&ci, 0, sizeof(ci));

    static char client_id[16];
    snprintf(client_id, sizeof(client_id), "picow-%d", rand());

    ci.client_id = client_id;
    ci.client_user = MQTT_BROKER_USERNAME;
    ci.client_pass = MQTT_BROKER_PASSWORD;
    ci.keep_alive = 60;

    ip_addr_t broker_addr;
    ip4addr_aton(MQTT_BROKER_IP, &broker_addr);

    cyw43_arch_lwip_begin();

    int err = mqtt_client_connect(client, &broker_addr, MQTT_BROKER_PORT,
        mqtt_connection_cb, 0, &ci);

    cyw43_arch_lwip_end();

    return err;
}

static void do_mqtt_subscribe(mqtt_client_t *client)
{
    const char *subscriptions[] = {
        "matrix_display/#",
        "transport_parser/#",
        NULL,
    };

    void *arg = NULL;
    int err;

    for (const char **s = subscriptions; *s; s++) {
        err = mqtt_subscribe(client, *s, 1, mqtt_sub_request_cb, arg);
        if(err != ERR_OK) {
            printf("mqtt_subscribe on %s return: %d\n", *s, err);
        }
        else {
            printf("mqtt_subscribe on %s success\n", *s);
        }
        vTaskDelay(10);
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
    }
}

static void mqtt_sub_request_cb(void *arg, err_t result)
{
    printf("Subscribe result: %d\n", result);
}

char current_topic[128];

static void mqtt_incoming_publish_cb(void *arg, const char *topic, u32_t tot_len)
{
    strncpy(current_topic, topic, sizeof(current_topic));

    printf("Topic is now: %s len: %d\n", current_topic, tot_len);
}

#define WEATHER_TOPIC "matrix_display/weather"
#define MEDIA_PLAYER_TOPIC "matrix_display/media_player"
#define CALENDAR_TOPIC "matrix_display/calendar"
#define SCROLLER_TOPIC "matrix_display/scroller"
#define TRANSPORT_TOPIC "matrix_display/transport"
#define PORCH_SENSOR_TOPIC "matrix_display/porch"
#define NOTIFICATION_TOPIC "matrix_display/notification"
#define SET_RTC_TIME_TOPIC "matrix_display/set_time"
#define BUZZER_PLAY_TOPIC "matrix_display/buzzer_play"
#define LIGHT_COMMAND_TOPIC "matrix_display/panel/switch"
#define LIGHT_BRIGHTNESS_COMMAND_TOPIC "matrix_display/panel/brightness/set"
#define SET_GRAYSCALE_TOPIC "matrix_display/greyscale/switch"
#define CONFIGURATION_TOPIC "matrix_display/configuration/"
#define AUTODISCOVER_CONTROL_TOPIC "matrix_display/autodiscover"

static void mqtt_incoming_data_cb(void *arg, const u8_t *data, u16_t len, u8_t flags)
{
    printf("Start of mqtt_incoming_data_cb(len: %d, flags: %d)\n", len, flags);
    static char data_as_chars[16384];
    static char *p = data_as_chars;

    memcpy(p, data, len);
    printf("Copy done\n");
    p += len;
    printf("P advanced\n");
    *p = '\0';
    printf("Null added\n");

    if (p - data_as_chars > 16384 - 1500) {
        panic("mqtt_incoming_data_cb(): data is too long");
    }

    printf("topic %s flags %d\n", current_topic, flags);

    if (flags & MQTT_DATA_FLAG_LAST) {
        if (strcmp(current_topic, WEATHER_TOPIC) == 0) {
            handle_weather_json_data(data_as_chars);
        }
        else if (strcmp(current_topic, MEDIA_PLAYER_TOPIC) == 0) {
            handle_media_player_json_data(data_as_chars);
        }
        else if (strcmp(current_topic, CALENDAR_TOPIC) == 0) {
            handle_calendar_data(data_as_chars);
        }
        else if (strcmp(current_topic, SCROLLER_TOPIC) == 0) {
            handle_scroller_json_data(data_as_chars);
        }
        else if (strcmp(current_topic, TRANSPORT_TOPIC) == 0) {
            handle_transport_json_data(data_as_chars);
        }
        else if( strcmp(current_topic, PORCH_SENSOR_TOPIC) == 0) {
            handle_porch_sensor_data(data_as_chars);
        }
        else if (strcmp(current_topic, NOTIFICATION_TOPIC) == 0) {
            handle_notificaiton_data(data_as_chars);
        }
        else if (strcmp(current_topic, SET_RTC_TIME_TOPIC) == 0) {
            handle_set_time_data(data_as_chars);
        }
        else if (strcmp(current_topic, BUZZER_PLAY_TOPIC) == 0) {
            handle_buzzer_play_data(data_as_chars);
        }
        else if (strcmp(current_topic, LIGHT_COMMAND_TOPIC) == 0) {
            handle_light_command_data(data_as_chars);
        }
        else if (strcmp(current_topic, LIGHT_BRIGHTNESS_COMMAND_TOPIC) == 0) {
            handle_light_brightness_command_data(data_as_chars);
        }
        else if (strcmp(current_topic, SET_GRAYSCALE_TOPIC) == 0) {
            handle_set_grayscale_data(data_as_chars);
        }
        else if (strncmp(current_topic, CONFIGURATION_TOPIC, strlen(CONFIGURATION_TOPIC)) == 0) {
            handle_configuration_data(current_topic + strlen(CONFIGURATION_TOPIC), data_as_chars);
        }
        else if (strcmp(current_topic, AUTODISCOVER_CONTROL_TOPIC) == 0) {
            handle_autodiscover_control_data(data_as_chars);
        }
        else if (strcmp(current_topic, TEMPERATURE_TOPIC) == 0) {
            printf("Ignoring temperature\n");
        }
#if BME680_PRESENT
        else if (strcmp(current_topic, PRESSURE_TOPIC) == 0) {
            printf("Ignoring pressure\n");
        }
        else if (strcmp(current_topic, HUMIDITY_TOPIC) == 0) {
            printf("Ignoring humidity\n");
        }
#endif
        else {
            printf("Unknown topic %s\n", current_topic);
        }
        memset(data_as_chars, 0, sizeof(data_as_chars));
        p = data_as_chars;
    }
    printf("Done incoming_data_cb()\n");
}

static cJSON *json_parser(char *data_as_chars)
{
    cJSON *json = cJSON_Parse(data_as_chars);

    if (json == NULL) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL) {
            printf("Error before: %s\n", error_ptr);
        }
        else {
            printf("Unknown JSON parse error\n");
        }
    }
    return json;
}

static message_anim_t message_anim;

static void handle_weather_json_data(char *data_as_chars)
{
    printf("Weather update\n");

    static weather_data_t weather_data;

    cJSON *json = json_parser(data_as_chars);
    if (json == NULL) {
        return;
    }
    else {
        cJSON *inside_temperatures = cJSON_GetObjectItem(json, "inside_temperatures");
        if (!inside_temperatures) {
            printf("Missing inside_temperatures\n");
            cJSON_Delete(json);
            return;
        }

        for (int i = 0; i < cJSON_GetArraySize(inside_temperatures) && i < NO_INSIDE_TEMPERATURES; i++) {
            cJSON *item = cJSON_GetArrayItem(inside_temperatures, i);
            cJSON *name = cJSON_GetObjectItem(item, "name");
            cJSON *temperature = cJSON_GetObjectItem(item, "temperature");

            if (!name || !temperature) {
                printf("Missing field(s) in inside_temperatures\n");
                cJSON_Delete(json);
                return;
            }

            strncpy(weather_data.inside_temperatures[i].name, cJSON_GetStringValue(name),
                sizeof(weather_data.inside_temperatures[i].name));
            weather_data.inside_temperatures[i].temperature = cJSON_GetNumberValue(temperature);

            weather_data.inside_temperatures_count = i + 1;
        }

        cJSON *forecasts = cJSON_GetObjectItem(json, "forecasts");
        if (!forecasts) {
            printf("Missing forecasts");
            cJSON_Delete(json);
            return;
        }
        for (int i = 0; i < cJSON_GetArraySize(forecasts) && i < NO_FORECASTS; i++) {
            cJSON *item = cJSON_GetArrayItem(forecasts, i);
            cJSON *datetime = cJSON_GetObjectItem(item, "datetime");
            cJSON *condition = cJSON_GetObjectItem(item, "condition");
            cJSON *temperature = cJSON_GetObjectItem(item, "temperature");
            cJSON *precipitation_probability = cJSON_GetObjectItem(item, "precipitation_probability");
            if (!datetime || !condition || !temperature || !precipitation_probability) {
                printf("Missing field(s) in forecasts\n");
                cJSON_Delete(json);
                return;
            }

            strncpy(weather_data.forecasts[i].time, cJSON_GetStringValue(datetime) + 11, 2 + 1);
            strncpy(weather_data.forecasts[i].condition, cJSON_GetStringValue(condition),
                sizeof(weather_data.forecasts[i].condition));
            weather_data.forecasts[i].temperature = cJSON_GetNumberValue(temperature);
            weather_data.forecasts[i].precipitation_probability = cJSON_GetNumberValue(precipitation_probability);

            weather_data.forecasts_count = i + 1;
        }

        cJSON *condition = cJSON_GetObjectItem(json, "condition");
        cJSON *temperature = cJSON_GetObjectItem(json, "temperature");
        cJSON *humidity = cJSON_GetObjectItem(json, "humidity");
        cJSON *wind_speed = cJSON_GetObjectItem(json, "wind_speed");
        cJSON *wind_bearing = cJSON_GetObjectItem(json, "wind_bearing");
        cJSON *pressure = cJSON_GetObjectItem(json, "pressure");
        if (!condition || !temperature || !temperature ||
           !humidity || !wind_speed || !wind_bearing || !wind_bearing)
        {
            printf("Weather has missing field(s)\n");
            cJSON_Delete(json);
            return;
        }

        strncpy(weather_data.condition, cJSON_GetStringValue(condition),
                sizeof(weather_data.condition));
        weather_data.temperature = cJSON_GetNumberValue(temperature);
        weather_data.humidty = cJSON_GetNumberValue(humidity);
        weather_data.wind_speed = cJSON_GetNumberValue(wind_speed);
        weather_data.wind_bearing = cJSON_GetNumberValue(wind_bearing);
        weather_data.pressure = cJSON_GetNumberValue(pressure);

        cJSON_Delete(json);
    }

    message_anim = {
        message_type: MESSAGE_ANIM_WEATHER,
        weather_data: weather_data,
    };
    if (xQueueSend(animate_queue, &message_anim, 10) != pdTRUE) {
        printf("Could not send weather data; dropping\n");
    }

    memset(&weather_data, 0, sizeof(weather_data_t));
}

static void handle_media_player_json_data(char *data_as_chars)
{
    printf("Media player update\n");

    static media_player_data_t media_player_data;

    cJSON *json = json_parser(data_as_chars);

    if (json == NULL) {
        return;
    }

    if (cJSON_IsObject(json)) {
        cJSON *state = cJSON_GetObjectItem(json, "state");
        cJSON *title = cJSON_GetObjectItem(json, "title");
        cJSON *artist = cJSON_GetObjectItem(json, "artist");
        cJSON *album = cJSON_GetObjectItem(json, "album");

        if (!(state && title && artist && album)) {
            printf("Media player update has missing fields\n");
            cJSON_Delete(json);
            return;
        }

        strncpy(media_player_data.state, cJSON_GetStringValue(state),
            sizeof(media_player_data.state));
        strncpy(media_player_data.media_title, cJSON_GetStringValue(title),
            sizeof(media_player_data.media_title));
        strncpy(media_player_data.media_artist, cJSON_GetStringValue(artist),
            sizeof(media_player_data.media_artist));
        strncpy(media_player_data.media_album_name,  cJSON_GetStringValue(album),
            sizeof(media_player_data.media_album_name));
    }

    cJSON_Delete(json);

    message_anim = {
        message_type: MESSAGE_ANIM_MEDIA_PLAYER,
        media_player_data: media_player_data,
    };
    if (xQueueSend(animate_queue, &message_anim, 10) != pdTRUE) {
        printf("Could not send media_player data; dropping\n");
    }
}

static void handle_calendar_data(char *data_as_chars)
{
    printf("Calendar update\n");

    static calendar_data_t calendar_data;

    cJSON *json = json_parser(data_as_chars);

    if (json == NULL) {
        return;
    }

    if (cJSON_IsArray(json)) {
        for (int i = 0; i < cJSON_GetArraySize(json) && i < NO_APPOINTMENTS; i++) {
            appointment_t *app = &calendar_data.appointments[i];

            cJSON *item = cJSON_GetArrayItem(json, i);
            cJSON *summary = cJSON_GetObjectItem(item, "summary");
            cJSON *start = cJSON_GetObjectItem(item, "start");

            if (!(summary && start)) {
                printf("Calendar update has missing fields\n");
                cJSON_Delete(json);
                return;
            }

            strncpy(app->summary, cJSON_GetStringValue(summary), sizeof(app->summary));

            const char *value = cJSON_GetStringValue(start);
            if (strlen(value) < 25) {
                printf("Start datetime is badly formed: %s", value);
                cJSON_Delete(json);
                return;
            }
            else {
                strncpy(app->start, &value[5], 5);
                app->start[5] = '\0';
                strncat(app->start, " ", 2);
                strncat(app->start, &value[11], 5);
            }
        }
    }

    cJSON_Delete(json);

    message_anim = {
        message_type: MESSAGE_ANIM_CALENDAR,
        calendar_data: calendar_data,
    };

    if (xQueueSend(animate_queue, &message_anim, 10) != pdTRUE) {
        printf("Could not send calendar data; dropping\n");
    }
}

static void handle_scroller_json_data(char *data_as_chars)
{
    printf("Scroller update %s\n", data_as_chars);

    static scroller_data_t scroller_data;

    cJSON *json = json_parser(data_as_chars);

    if (json == NULL) {
        return;
    }

    if (cJSON_IsArray(json)) {
        for (int i = 0; i < cJSON_GetArraySize(json) && i < NO_SCROLLERS; i++) {
            cJSON *item = cJSON_GetArrayItem(json, i);
            strncpy(scroller_data.text[i], cJSON_GetStringValue(item), 256);
        }
        scroller_data.array_size = cJSON_GetArraySize(json);
    }
    else {
        printf("Not an arrray in scroller data\n");
    }

    cJSON_Delete(json);

    message_anim_t message_anim = {
        message_type: MESSAGE_ANIM_SCROLLER,
        scroller_data: scroller_data,
    };

    if (xQueueSend(animate_queue, &message_anim, 10) != pdTRUE) {
        printf("Could not send scroller data; dropping\n");
    }
}

static void handle_transport_json_data(char *data_as_chars)
{
    printf("Transport update %s\n", data_as_chars);

    static transport_data_t transport_data;

    cJSON *json = json_parser(data_as_chars);

    if (json == NULL) {
        return;
    }

    if (cJSON_IsArray(json)) {
        for (int i = 0; i < cJSON_GetArraySize(json) && i < NO_TRANSPORT_JOURNIES; i++) {
            cJSON *item = cJSON_GetArrayItem(json, i);
            cJSON *towards = cJSON_GetObjectItem(item, "Towards");
            cJSON *departures_summary = cJSON_GetObjectItem(item, "DeparturesSummary");

            if (!towards || !departures_summary) {
                printf("Transport missing field(s)");
                return;
            }

            strncpy(transport_data.journies[i].towards, cJSON_GetStringValue(towards), 16);
            strncpy(transport_data.journies[i].departures_summary, cJSON_GetStringValue(departures_summary), 64);
        }

        message_anim_t message_anim = {
            message_type: MESSAGE_ANIM_TRANSPORT,
            transport_data: transport_data,
        };
        printf("Sending transport data\n");
        if (xQueueSend(animate_queue, &message_anim, 10) != pdTRUE) {
            printf("Could not send transport data; dropping\n");
        }
    }
    else {
        printf("Not an arrray in transport data\n");
    }

    cJSON_Delete(json);
}

static void handle_porch_sensor_data(char *data_as_chars)
{
    printf("Porch update: %s\n", data_as_chars);

    porch_t porch = {
        occupied: (strcmp(data_as_chars, "on") == 0) ? true : false,
    };

    message_anim = {
        message_type: MESSAGE_ANIM_PORCH,
        porch: porch,
    };
    if (xQueueSend(animate_queue, &message_anim, 10) != pdTRUE) {
        printf("Could not send media_player data; dropping\n");
    }

    if (porch.occupied) {
        message_buzzer_t message_buzzer = {
            message_type: MESSAGE_BUZZER_PLAY,
            play_type: BUZZER_PLAY_PORCH,
        };
        if (xQueueSend(buzzer_queue, &message_buzzer, 10) != pdTRUE) {
            printf("Could not send message_buzzer data; dropping\n");
        }
    }
}

static void handle_notificaiton_data(char *data_as_chars)
{
    printf("Notifation update: %s", data_as_chars);

    message_anim = {
        message_type: MESSAGE_ANIM_NOTIFICATION,
    };

    strncpy(message_anim.notification.text, data_as_chars, sizeof(message_anim.notification.text));

    if (xQueueSend(animate_queue, &message_anim, 10) != pdTRUE) {
        printf("Could not send weather data; dropping");
    }

    message_buzzer_t message_buzzer = {
        message_type: MESSAGE_BUZZER_PLAY,
        play_type: BUZZER_PLAY_NOTIFICATION,
    };
    if (xQueueSend(buzzer_queue, &message_buzzer, 10) != pdTRUE) {
        printf("Could not send message_buzzer data; dropping\n");
    }
}

static void handle_set_time_data(char *data_as_chars)
{
    printf("handle_set_time_data()\n");

    ds3231_t ds3231;

    bcd_string_to_bytes(data_as_chars, ds3231.datetime_buffer, DS3231_DATETIME_LEN);

    if (xQueueSend(i2c_queue, &ds3231, 10) != pdTRUE) {
        printf("Could not send ds3231 data; dropping\n");
    }
}

static void handle_buzzer_play_data(char *data_as_chars)
{
    printf("handle_buzzer_data()\n");

    message_buzzer_t message_buzzer = {
        .message_type = MESSAGE_BUZZER_PLAY,
    };

    bcd_string_to_bytes(data_as_chars, &message_buzzer.play_type, sizeof(uint8_t));

    if (xQueueSend(buzzer_queue, &message_buzzer, 10) != pdTRUE) {
        printf("Could not send buzzer data; dropping");
    }
}

static void handle_light_command_data(char *data_as_chars)
{
    message_anim = {
        message_type: MESSAGE_ANIM_BRIGHTNESS,
        brightness: 0,
    };

    if (xQueueSend(animate_queue, &message_anim, 10) != pdTRUE) {
        printf("Could not send light command data; dropping");
    }
}

static void handle_light_brightness_command_data(char *data_as_chars)
{
    printf("Brightness set: %s\n", data_as_chars);

    char *end = NULL;
    int brightness = strtol(data_as_chars, &end, 10);

    if (!end || brightness < 0 || brightness > 255) {
        printf("Brightness out of range or invalid");
        return;
    }

    message_anim = {
        message_type: MESSAGE_ANIM_BRIGHTNESS,
        brightness: (uint8_t) brightness,
    };

    if (xQueueSend(animate_queue, &message_anim, 10) != pdTRUE) {
        printf("Could not send brightness data; dropping");
    }
}

static void handle_set_grayscale_data(char *data_as_chars)
{
    printf("Grayscale update: %\ns", data_as_chars);

    message_anim = {
        message_type: MESSAGE_ANIM_GRAYSCALE,
    };

    if (strcmp(data_as_chars, "ON") == 0) {
        message_anim.grayscale = true;
    } else if (strcmp(data_as_chars, "OFF") == 0) {
        message_anim.grayscale = false;
    } else {
        printf("Malformed grayscale %s\n", data_as_chars);
        return;
    }

    if (xQueueSend(animate_queue, &message_anim, 10) != pdTRUE) {
        printf("Could not send grayscale data; dropping");
    }
}

static void handle_configuration_data(char *attribute, char *data_as_chars)
{
    printf("Configuration update\n");

    configuration_t configuration;

    configuration.clock_colon_flash = -1;
    configuration.clock_duration = -1;
    configuration.inside_temperatures_scroll_speed = -1;
    configuration.current_weather_duration = -1;
    configuration.weather_forecast_duration = -1;
    configuration.media_player_scroll_speed = -1;
    configuration.calendar_scroll_speed = -1;
    configuration.transport_duration = -1;
    configuration.scroller_interval = -1;
    configuration.scroller_speed = -1;
    configuration.snowflake_count = -1;

    char *end = NULL;
    int value = strtol(data_as_chars, &end, 10);

    if (!end) {
        printf("Could not convert configuration data to number\n");
        return;
    }

    // Update just the changing field
    if (strcmp(attribute, "clock_colon_flash") == 0) {
        configuration.clock_colon_flash = value;
    }
    else if (strcmp(attribute, "clock_duration") == 0) {
        configuration.clock_duration = value;
    }
    else if (strcmp(attribute, "inside_temperatures_scroll_speed") == 0) {
        configuration.inside_temperatures_scroll_speed = value;
    }
    else if (strcmp(attribute, "current_weather_duration") == 0) {
        configuration.current_weather_duration = value;
    }
    else if (strcmp(attribute, "weather_forecast_duration") == 0) {
        configuration.weather_forecast_duration = value;
    }
    else if (strcmp(attribute, "media_player_scroll_speed") == 0) {
        configuration.media_player_scroll_speed = value;
    }
    else if (strcmp(attribute, "calendar_scroll_speed") == 0) {
        configuration.calendar_scroll_speed = value;
    }
    else if (strcmp(attribute, "transport_duration") == 0) {
        configuration.transport_duration = value;
    }
    else if (strcmp(attribute, "scroller_interval") == 0) {
        configuration.scroller_interval = value;
    }
    else if (strcmp(attribute, "scroller_speed") == 0) {
        configuration.scroller_speed = value;
    }
    else if (strcmp(attribute, "snowflake_count") == 0) {
        configuration.snowflake_count = value;
    }
    else {
        printf("Unknown configuration attribute: %s", attribute);
    }

    message_anim = {
        message_type: MESSAGE_ANIM_CONFIGURATION,
        configuration: configuration,
    };

    if (xQueueSend(animate_queue, &message_anim, 10) != pdTRUE) {
        printf("Could not send configuration data; dropping\n");
    }
}

bool autodisover_enable = false;
bool send_autodiscover = false;
static void handle_autodiscover_control_data(char *data_as_chars)
{
    printf("AutoDiscover control\n");

    autodisover_enable = strcmp(data_as_chars, "ON") == 0 ? true : false;

    send_autodiscover = true;
}

static void dump_weather_data(weather_data_t *weather_data)
{
    printf("--------------\n");
    printf("Current: condition=%s temperature=%.1f humidity=%.1f\n",
        weather_data->condition, weather_data->temperature, weather_data->humidty);
    for (int i = 0; i < weather_data->forecasts_count; i++) {
        printf("%s: condition=%s temperature=%.1f percipitation_probability=%.1f\n",
            weather_data->forecasts[i].time, weather_data->forecasts[i].condition,
            weather_data->forecasts[i].temperature,
            weather_data->forecasts[i].precipitation_probability);
    }
    printf("--------------\n");
}

cJSON *create_base_object(const char *name, const char *unique_id)
{
    cJSON *obj = cJSON_CreateObject();
    cJSON_AddItemToObject(obj, "name", cJSON_CreateString(name));
    cJSON_AddItemToObject(obj, "retain", cJSON_CreateBool(true));
    cJSON_AddItemToObject(obj, "unique_id", cJSON_CreateString(unique_id));

    return obj;
}

// Add the device, print the object into a string, free the object, and then publish it at the given topic, before
// freeing the serialised string. Return the error code from the publishing.
int publish_object_as_device_entity(cJSON *obj, cJSON *device, mqtt_client_t *client, const char *topic)
{
    cJSON_AddItemReferenceToObject(obj, "device", device);
    char *json_chars = cJSON_PrintUnformatted(obj);
    cJSON_free(obj);

    cyw43_arch_lwip_begin();

    int err = mqtt_publish(client, topic, json_chars, strlen(json_chars), 0, 1, mqtt_pub_request_cb, NULL);
    free(json_chars);

    cyw43_arch_lwip_end();
    vTaskDelay(10 / portTICK_PERIOD_MS);

    return err;
}

static void publish_loop_body(mqtt_client_t *client)
{
    static message_mqtt_t message_mqtt;

    if (mqtt_client_is_connected(client)) {
        if (xQueueReceive(mqtt_queue, &message_mqtt, 0) == pdTRUE) {
            switch (message_mqtt.message_type) {
                char temperature_buffer[10];
#if BME680_PRESENT
                char pressure_buffer[10];
                char humidity_buffer[10];
#endif
                int err;
                case MESSAGE_MQTT_CLIMATE:
                    snprintf(temperature_buffer, sizeof(temperature_buffer), "%.2f", message_mqtt.climate.temperature);
                    printf("Got temperature: %s\n", temperature_buffer);

#if BME680_PRESENT
                    snprintf(pressure_buffer, sizeof(pressure_buffer), "%.2f", message_mqtt.climate.pressure);
                    printf("Got pressure: %s\n", pressure_buffer);
                    snprintf(humidity_buffer, sizeof(humidity_buffer), "%.2f", message_mqtt.climate.humidity);
                    printf("Got humidity: %s\n", humidity_buffer);
#endif

                    cyw43_arch_lwip_begin();
                    err = mqtt_publish(client, TEMPERATURE_TOPIC, temperature_buffer, strlen(temperature_buffer), 0, 1,
                        mqtt_pub_request_cb, NULL);
                    if (err != ERR_OK) {
                        printf("mqtt_publish on %s return: %d\n", TEMPERATURE_TOPIC, err);
                    }

#if BME680_PRESENT
                    err = mqtt_publish(client, PRESSURE_TOPIC, pressure_buffer, strlen(pressure_buffer), 0, 1,
                        mqtt_pub_request_cb, NULL);
                    if (err != ERR_OK) {
                        printf("mqtt_publish on %s return: %d\n", PRESSURE_TOPIC, err);
                    }
                    err = mqtt_publish(client, HUMIDITY_TOPIC, humidity_buffer, strlen(humidity_buffer), 0, 1,
                        mqtt_pub_request_cb, NULL);
                    if (err != ERR_OK) {
                        printf("mqtt_publish on %s return: %d\n", HUMIDITY_TOPIC, err);
                    }
#endif

                    cyw43_arch_lwip_end();
                    break;

                default:
                    panic("Invalid message type (%d)", message_mqtt.message_type);
                    break;
            }
        }

        if (send_autodiscover) {
            int err = 0;
            char *json_chars = NULL;
            if (autodisover_enable) {
                cJSON *device = cJSON_CreateObject();
                cJSON_AddItemToObject(device, "name", cJSON_CreateString("Matrix Display"));
                cJSON_AddItemToObject(device, "identifiers", cJSON_CreateString("matrix_display"));
                cJSON_AddItemToObject(device, "manufacturer", cJSON_CreateString("lawrence@aslak.net"));
                cJSON_AddItemToObject(device, "model", cJSON_CreateString("Matrix Display 64x32"));

                cJSON *light = create_base_object("Panel", "matrix_display_brightness");
                cJSON_AddItemToObject(light, "command_topic", cJSON_CreateString(LIGHT_COMMAND_TOPIC));
                cJSON_AddItemToObject(light, "payload_off", cJSON_CreateString("OFF"));
                cJSON_AddItemToObject(light, "brightness_command_topic", cJSON_CreateString(LIGHT_BRIGHTNESS_COMMAND_TOPIC));
                cJSON_AddItemToObject(light, "on_command_type", cJSON_CreateString("brightness"));
                err += publish_object_as_device_entity(light, device, client, "homeassistant/light/matrix_display/config")
                    != ERR_OK ? 1 : 0;

                cJSON *grayscale = create_base_object("Grayscale", "matrix_display_grayscale");
                cJSON_AddItemToObject(grayscale, "command_topic", cJSON_CreateString(SET_GRAYSCALE_TOPIC));
                err += publish_object_as_device_entity(grayscale, device, client, "homeassistant/switch/matrix_display/config")
                    != ERR_OK ? 1 : 0;

                cJSON *temp = create_base_object("Temperature", "matrix_display_temperature");
                cJSON_AddItemToObject(temp, "state_topic", cJSON_CreateString(TEMPERATURE_TOPIC));
                cJSON_AddItemToObject(temp, "unit_of_measurement", cJSON_CreateString("°C"));
                err += publish_object_as_device_entity(temp, device, client, "homeassistant/sensor/matrix_display_temperature/config")
                    != ERR_OK ? 1 : 0;

#if BME680_PRESENT
                cJSON *pressure = create_base_object("Pressure", "matrix_display_pressure");
                cJSON_AddItemToObject(pressure, "state_topic", cJSON_CreateString(PRESSURE_TOPIC));
                cJSON_AddItemToObject(pressure, "unit_of_measurement", cJSON_CreateString("hPa"));
                err += publish_object_as_device_entity(pressure, device, client, "homeassistant/sensor/matrix_display_pressure/config")
                     != ERR_OK ? 1 : 0;

                cJSON *humidity = create_base_object("Humidity", "matrix_display_humidity");
                cJSON_AddItemToObject(humidity, "state_topic", cJSON_CreateString(HUMIDITY_TOPIC));
                cJSON_AddItemToObject(humidity, "unit_of_measurement", cJSON_CreateString("%"));
                err += publish_object_as_device_entity(humidity, device, client, "homeassistant/sensor/matrix_display_humidity/config")
                     != ERR_OK ? 1 : 0;

#endif
                cJSON *snowflakes = create_base_object("Snowflake Count", "matrix_display_snowflake_count");
                cJSON_AddItemToObject(snowflakes, "command_topic", cJSON_CreateString("matrix_display/configuration/snowflake_count"));
                cJSON_AddNumberToObject(snowflakes, "min", 0.0);
                cJSON_AddNumberToObject(snowflakes, "max", 255.0);
                err += publish_object_as_device_entity(snowflakes, device, client, "homeassistant/number/matrix_display/config")
                     != ERR_OK ? 1 : 0;

                cJSON_free(device);
            }
            else {
                cyw43_arch_lwip_begin();

                err += mqtt_publish(client, "homeassistant/light/matrix_display/config", "", 0,
                    0, 1, mqtt_pub_request_cb, NULL) != ERR_OK ? 1 : 0;
                err += mqtt_publish(client, "homeassistant/switch/matrix_display/config", "", 0,
                    0, 1, mqtt_pub_request_cb, NULL) != ERR_OK ? 1 : 0;
                err += mqtt_publish(client, "homeassistant/sensor/matrix_display/config", "", 0,
                    0, 1, mqtt_pub_request_cb, NULL) != ERR_OK ? 1 : 0;
                err += mqtt_publish(client, "homeassistant/number/matrix_display/config", "", 0,
                    0, 1, mqtt_pub_request_cb, NULL) != ERR_OK ? 1 : 0;

                cyw43_arch_lwip_end();
            }

            if (err > 0) {
                printf("mqtt_publish returned %d errors\n", err);
            }

            send_autodiscover = false;
        }

        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    else {
        if (mqtt_client_is_connected(client) == 0) {
            printf("MQTT not connected; reconnecting\n");
            do_mqtt_connect(client);
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
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