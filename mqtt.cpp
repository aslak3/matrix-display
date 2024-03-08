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
extern QueueHandle_t rtc_queue;
extern QueueHandle_t buzzer_queue;

void led_task(void *dummy);

static void connect_wifi(void);
static int do_mqtt_connect(mqtt_client_t *client);
static void do_mqtt_subscribe(mqtt_client_t *client);
static void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status);
static void mqtt_sub_request_cb(void *arg, err_t result);
static void mqtt_pub_request_cb(void *arg, err_t result);
static void mqtt_incoming_publish_cb(void *arg, const char *topic, u32_t tot_len);

static void handle_weather_data(char *data_as_chars);
static void handle_media_player_data(char *attribute, char *data_as_chars);
static void handle_calendar_data(char *attribute, char *data_as_chars);
static void handle_bluestar_data(char *data_as_chars);
static void handle_porch_sensor_data(char *data_as_chars);
static void handle_notificaiton_data(char *data_as_chars);
static void handle_set_rtc_time_data(char *data_as_chars);
static void handle_buzzer_play_data(char *data_as_chars);
static void handle_set_brightness_data(char *attribute, char *data_as_chars);
static void handle_set_grayscale_data(char *data_as_chars);
static void handle_set_snowflakes_data(char *data_as_chars);
static void handle_configuration_data(char *attribute, char *data_as_chars);

static void dump_weather_data(weather_data_t *weather_data);

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
        "home/weather",
        "homeassistant/media_player/squeezebox/state",
        "homeassistant/media_player/squeezebox/media_title",
        "homeassistant/media_player/squeezebox/media_artist",
        "homeassistant/media_player/squeezebox/media_album_name",
        "homeassistant/binary_sensor/porch_motion_sensor_iaszone/state",
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
        else {
            printf("mqtt_subscribe on %s success\n", *s);
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
            else {
                printf("mqtt_subscribe on %s success\n", subscription);
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

#define WEATHER_TOPIC "home/weather"
#define MEDIA_PLAYER_TOPIC "homeassistant/media_player/squeezebox/"
#define CALENDAR_TOPIC "homeassistant/sensor/ical_our_house_event_"
#define BUS_JOURNIES "bluestar_parser/journies"
#define PORCH_SENSOR_TOPIC "homeassistant/binary_sensor/porch_motion_sensor_iaszone/state"
#define NOTIFICATION_TOPIC "matrix_display/notification"
#define SET_RTC_TIME_TOPIC "matrix_display/set_rtc_time"
#define BUZZER_PLAY_TOPIC "matrix_display/buzzer_play"
// Includes brightness and brightness_red etc
#define SET_BRIGHTNESS_TOPIC "matrix_display/brightness/"
#define SET_GRAYSCALE_TOPIC "matrix_display/grayscale"
#define CONFIGURATION_TOPIC "matrix_display/configuration/"

static void mqtt_incoming_data_cb(void *arg, const u8_t *data, u16_t len, u8_t flags)
{
    printf("Start of mqtt_incoming_data_cb(flags: %d)\n", flags);
    static char data_as_chars[16384];
    static char *p = data_as_chars;

    memcpy(p, data, len);
    p += len;
    *p = '\0';

    if (p - data_as_chars > 16384 - 1500) {
        panic("mqtt_incoming_data_cb(): data is too long");
    }

    printf("topic %s flags %d\n", current_topic, flags);

    if (flags & MQTT_DATA_FLAG_LAST) {
        if (strcmp(current_topic, WEATHER_TOPIC) == 0) {
            handle_weather_data(data_as_chars);
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
        else if (strcmp(current_topic, BUZZER_PLAY_TOPIC) == 0) {
            handle_buzzer_play_data(data_as_chars);
        }
        else if (strncmp(current_topic, SET_BRIGHTNESS_TOPIC, strlen(SET_BRIGHTNESS_TOPIC)) == 0) {
            handle_set_brightness_data(current_topic + strlen(SET_BRIGHTNESS_TOPIC), data_as_chars);
        }
        else if (strcmp(current_topic, SET_GRAYSCALE_TOPIC) == 0) {
            handle_set_grayscale_data(data_as_chars);
        }
        else if (strncmp(current_topic, CONFIGURATION_TOPIC, strlen(CONFIGURATION_TOPIC)) == 0) {
            handle_configuration_data(current_topic + strlen(CONFIGURATION_TOPIC), data_as_chars);
        }
        else {
            printf("Unknown topic %s\n", current_topic);
        }
        memset(data_as_chars, 0, sizeof(data_as_chars));
        p = data_as_chars;
    }
    // printf("Done incoming_data_cb()\n");
}

static message_anim_t message_anim;

static void handle_weather_data(char *data_as_chars)
{
    printf("Weather update\n");

    static weather_data_t weather_data;

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
        cJSON *inside_temperatures = cJSON_GetObjectItem(json, "inside_temperatures");
        if (!inside_temperatures) {
               panic("Missing inside_temperatures");
        }

        for (int i = 0; i < cJSON_GetArraySize(inside_temperatures) && i < NO_INSIDE_TEMPERATURES; i++) {
            cJSON *item = cJSON_GetArrayItem(inside_temperatures, i);
            cJSON *name = cJSON_GetObjectItem(item, "name");
            cJSON *temperature = cJSON_GetObjectItem(item, "temperature");
        
            if (!name || !temperature) {
                panic("Missing field(s) in inside_temperatures");
            }

            strncpy(weather_data.inside_temperatures[i].name, cJSON_GetStringValue(name),
                sizeof(weather_data.inside_temperatures[i].name));
            weather_data.inside_temperatures[i].temperature = cJSON_GetNumberValue(temperature);

            weather_data.inside_temperatures_count = i + 1;
        }

        cJSON *forecasts = cJSON_GetObjectItem(json, "forecasts");
        if (!forecasts) {
               panic("Missing forecasts");
        }
        for (int i = 0; i < cJSON_GetArraySize(forecasts) && i < NO_FORECASTS; i++) {
            cJSON *item = cJSON_GetArrayItem(forecasts, i);
            cJSON *datetime = cJSON_GetObjectItem(item, "datetime");
            cJSON *condition = cJSON_GetObjectItem(item, "condition");
            cJSON *temperature = cJSON_GetObjectItem(item, "temperature");
            cJSON *precipitation_probability = cJSON_GetObjectItem(item, "precipitation_probability");
            if (!datetime || !condition || !temperature || !precipitation_probability) {
                panic("Missing field(s) in forecasts");
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
            panic("Missing field(s)");
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
        message_anim = {
            message_type: MESSAGE_ANIM_MEDIA_PLAYER,
            media_player_data: media_player_data,
        };
        if (xQueueSend(animate_queue, &message_anim, 10) != pdTRUE) {
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

    message_anim = {
        message_type: MESSAGE_ANIM_CALENDAR,
        calendar_data: calendar_data,
    };
    // printf("Sending calendar data\n");
    if (xQueueSend(animate_queue, &message_anim, 10) != pdTRUE) {
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

        message_anim_t message_anim = {
            message_type: MESSAGE_ANIM_BLUESTAR,
            bluestar_data: bluestar_data,
        };
        printf("Sending bluestar data\n");
        if (xQueueSend(animate_queue, &message_anim, 10) != pdTRUE) {
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

static void handle_set_rtc_time_data(char *data_as_chars)
{
    printf("handle_set_rtc_time_data()\n");

    rtc_t rtc;

    bcd_string_to_bytes(data_as_chars, rtc.datetime_buffer, RTC_DATETIME_LEN);

    if (xQueueSend(rtc_queue, &rtc, 10) != pdTRUE) {
        printf("Could not send rtc data; dropping");
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

    message_anim = {
        message_type: MESSAGE_ANIM_BRIGHTNESS,
        brightness: brightness,
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
        message_anim.grayscale = false;
        printf("Malformed grayscale %s\n", data_as_chars);
    }

    if (xQueueSend(animate_queue, &message_anim, 10) != pdTRUE) {
        printf("Could not send grayscale data; dropping");
    }
}

static void handle_configuration_data(char *attribute, char *data_as_chars)
{
    printf("Configuration update\n");

    configuration_t configuration;

    configuration.rtc_duration = -1;
    configuration.inside_temperatures_scroll_speed = -1;
    configuration.current_weather_duration = -1;
    configuration.weather_forecast_duration = -1;
    configuration.media_player_scroll_speed = -1;
    configuration.calendar_scroll_speed = -1;
    configuration.bluestar_duration = -1;
    configuration.scroller_interval = -1;
    configuration.scroller_speed = -1;
    configuration.snowflake_count = -1;

    int value = atol(data_as_chars);

    // Update just the changing field
    if (strcmp(attribute, "rtc_duration") == 0) {
        configuration.rtc_duration = value;
    }
    if (strcmp(attribute, "inside_temperatures_scroll_speed") == 0) {
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
    else if (strcmp(attribute, "bluestar_duration") == 0) {
        configuration.bluestar_duration = value;
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
        printf("Could not send configuration data; dropping");
    }
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

static void publish_loop_body(mqtt_client_t *client)
{
    static message_mqtt_t message_mqtt;

    if (mqtt_client_is_connected(client)) {
        if (xQueueReceive(mqtt_queue, &message_mqtt, portTICK_PERIOD_MS * 10) == pdTRUE) {
            switch (message_mqtt.message_type) {
                char buffer[10];
                int err;
                case MESSAGE_MQTT_TEMPERATURE:
                    snprintf(buffer, sizeof(buffer), "%.2f", message_mqtt.temperature);
                    printf("Got a temperature: %s\n", buffer);

                    cyw43_arch_lwip_begin();
                    err = mqtt_publish(client, TEMPERATURE_TOPIC, buffer, strlen(buffer), 0, 1, mqtt_pub_request_cb, NULL);
                    if (err != ERR_OK) {
                        printf("mqtt_publish on %s return: %d\n", TEMPERATURE_TOPIC, err);
                    }
                    cyw43_arch_lwip_end();
                    break;

                default:
                    panic("Invalid message type (%d)", message_mqtt.message_type);
                    break;
            }
        }
    }
    else {
        if (mqtt_client_is_connected(client) == 0) {
            printf("MQTT not connected; reconnecting\n");
            do_mqtt_connect(client);
        }
        vTaskDelay(100 * portTICK_PERIOD_MS);
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