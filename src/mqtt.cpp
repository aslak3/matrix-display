#include <stdio.h>
#include <string.h>

#if PICO_SDK
#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>

#include <pico/stdlib.h>
#include <pico/cyw43_arch.h>
#else
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "driver/gpio.h"
#include "esp_wifi.h"
#endif

#include "mqtt_opts.h"
#include <lwip/apps/mqtt.h>

#include "matrix_display.h"

#include "messages.h"

#include "cJSON.h"

#if ESP32_SDK
#define LED_GPIO GPIO_NUM_13
#endif

extern QueueHandle_t mqtt_queue;
extern QueueHandle_t animate_queue;
extern QueueHandle_t i2c_queue;
extern QueueHandle_t buzzer_queue;

void led_task(void *dummy);

static void connect_wifi(const char *ssid, const char *password);
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
static void handle_buzzer_play_rtttl_data(char *data_as_chars);
static void handle_light_command_data(char *data_as_chars);
static void handle_light_brightness_command_data(char *data_as_chars);
static void handle_set_grayscale_data(char *data_as_chars);
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
    DEBUG_printf("%s: core%u\n", pcTaskGetName(NULL), get_core_num());
#endif

#if ESP32_SDK
    gpio_reset_pin(LED_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
#endif

    while (true) {
#if PICO_SDK
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
#else
        gpio_set_level(LED_GPIO, true);
#endif
        vTaskDelay(1000);
#if PICO_SDK
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
#else
        gpio_set_level(LED_GPIO, false);
#endif
        vTaskDelay(1000);
    }
}

#if PICO_SDK

static void connect_wifi(const char *ssid, const char *password)
{
    while (1) {
        if (cyw43_arch_init_with_country(CYW43_COUNTRY_UK)) {
            panic("failed to initialise\n");
        }
        cyw43_arch_enable_sta_mode();

        DEBUG_printf("Connecting to WiFi...\n");
        if (cyw43_arch_wifi_connect_blocking(ssid, password, CYW43_AUTH_WPA2_MIXED_PSK)) {
            DEBUG_printf("failed to connect.\n");
            cyw43_arch_deinit();
        } else {
            DEBUG_printf("Connected.\n");
            break;
        }
    }
}

#else

void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id,
    void *event_data);

bool networking_available = false;

static void connect_wifi(const char *ssid, const char *password)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
        ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
        IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &instance_got_ip));

    wifi_config_t wifi_config;
    memset(&wifi_config, 0, sizeof(wifi_config));
    memcpy(wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    memcpy(wifi_config.sta.password, ssid, sizeof(wifi_config.sta.password));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    while (!networking_available) {
        vTaskDelay(100 / portTICK_PERIOD_MS);
        DEBUG_printf("Waiting for networking...\n");
    }

    DEBUG_printf("Wi-Fi initialization complete.\n");
}

void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id,
    void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        networking_available = false;
        DEBUG_printf("Disconnected. Reconnecting...\n");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        DEBUG_printf("Got IP: %d.%d.%d.%d\n", IP2STR(&event->ip_info.ip));
        networking_available = true;
    }
}

#endif

void mqtt_task(void *dummy)
{
#if FREE_RTOS_KERNEL_SMP
    vTaskCoreAffinitySet(NULL, 1 << 0);
    DEBUG_printf("%s: core%u\n", pcTaskGetName(NULL), get_core_num());
#endif

    xTaskCreate(&led_task, "LED Task", 256, NULL, 0, NULL);

    connect_wifi(WIFI_SSID, WIFI_PASSWORD);

    mqtt_client_t *client = mqtt_client_new();

    /* Setup callback for incoming publish requests */
    mqtt_set_inpub_callback(client, mqtt_incoming_publish_cb, mqtt_incoming_data_cb, NULL);

    int err = do_mqtt_connect(client);

    /* For now just print the result code if something goes wrong */
    if (err != ERR_OK) {
        DEBUG_printf("do_mqtt_connect() return %d\n", err);
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

#if PICO_SDK
    cyw43_arch_lwip_begin();
#endif

    int err = mqtt_client_connect(client, &broker_addr, MQTT_BROKER_PORT,
        mqtt_connection_cb, 0, &ci);

#if PICO_SDK
    cyw43_arch_lwip_end();
#endif

    return err;
}

#define TEMPERATURE_TOPIC "matrix_display/temperature"
#if BME680_PRESENT
#define PRESSURE_TOPIC "matrix_display/pressure"
#define HUMIDITY_TOPIC "matrix_display/humidity"
#endif
#define WEATHER_TOPIC "matrix_display/weather"
#define MEDIA_PLAYER_TOPIC "matrix_display/media_player"
#define CALENDAR_TOPIC "matrix_display/calendar"
#define SCROLLER_TOPIC "matrix_display/scroller"
#define TRANSPORT_TOPIC "matrix_display/transport"
#define PORCH_SENSOR_TOPIC "matrix_display/porch"
#define NOTIFICATION_TOPIC "matrix_display/notification"
#define SET_RTC_TIME_TOPIC "matrix_display/set_time"
#define BUZZER_PLAY_RTTTL_TOPIC "matrix_display/buzzer_play_rtttl"
#define LIGHT_COMMAND_TOPIC "matrix_display/panel/switch"
#define LIGHT_BRIGHTNESS_COMMAND_TOPIC "matrix_display/panel/brightness/set"
#define SET_GRAYSCALE_TOPIC "matrix_display/greyscale/switch"
#define CONFIGURATION_TOPIC "matrix_display/configuration/"
#define AUTODISCOVER_CONTROL_TOPIC "matrix_display/autodiscover"

static void do_mqtt_subscribe(mqtt_client_t *client)
{
    const char *subscriptions[] = {
        WEATHER_TOPIC,
        MEDIA_PLAYER_TOPIC,
        CALENDAR_TOPIC,
        SCROLLER_TOPIC,
        TRANSPORT_TOPIC,
        PORCH_SENSOR_TOPIC,
        NOTIFICATION_TOPIC,
        SET_RTC_TIME_TOPIC,
        BUZZER_PLAY_RTTTL_TOPIC,
        LIGHT_COMMAND_TOPIC,
        LIGHT_BRIGHTNESS_COMMAND_TOPIC,
        SET_GRAYSCALE_TOPIC,
        AUTODISCOVER_CONTROL_TOPIC,
        "matrix_display/configuration/#",
        NULL,
    };

    void *arg = NULL;
    int err;
    for (const char **s = subscriptions; *s; s++) {
        err = mqtt_subscribe(client, *s, 1, mqtt_sub_request_cb, arg);
        if(err != ERR_OK) {
            DEBUG_printf("mqtt_subscribe on %s return: %d\n", *s, err);
        }
        else {
            DEBUG_printf("mqtt_subscribe on %s success\n", *s);
        }
        vTaskDelay(10);
    }
}

static void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status)
{
    if (status == MQTT_CONNECT_ACCEPTED) {
        DEBUG_printf("Connected\n");
        do_mqtt_subscribe(client);
    }
    else {
        DEBUG_printf("Disconnected: %d\n", status);
        vTaskDelay(1000);
    }
}

static void mqtt_sub_request_cb(void *arg, err_t result)
{
    DEBUG_printf("Subscribe result: %d\n", result);
}

static char current_topic[128];

static void mqtt_incoming_publish_cb(void *arg, const char *topic, u32_t tot_len)
{
    strncpy(current_topic, topic, sizeof(current_topic));

    DEBUG_printf("Topic is now: %s len: %lu\n", current_topic, tot_len);
}

static char data_as_chars[4096];

static void mqtt_incoming_data_cb(void *arg, const u8_t *data, u16_t len, u8_t flags)
{
    static u16_t running_len = 0;

    DEBUG_printf("Start of mqtt_incoming_data_cb(len: %u, flags: %u)\n", len, flags);

#if PICO_SDK
    taskENTER_CRITICAL();
#endif

    // memcpy(data_as_chars + running_len, data, len);
    for (u16_t c = 0; c < len; c++) {
        data_as_chars[c + running_len] = data[c];
    }
    // DEBUG_printf("Copy done\n");
    running_len += len;
    data_as_chars[running_len] = '\0';
    // DEBUG_printf("length advanced to %d, Null added\n", running_len);

#if PICO_SDK
    taskEXIT_CRITICAL();
#endif

    DEBUG_printf("topic %s flags %d\n", current_topic, flags);

    if (running_len > sizeof(data_as_chars)) {
        panic("mqtt_incoming_data_cb(): data is too long");
    }

    if (flags & MQTT_DATA_FLAG_LAST) {
        running_len = 0;

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
        else if (strcmp(current_topic, BUZZER_PLAY_RTTTL_TOPIC) == 0) {
            handle_buzzer_play_rtttl_data(data_as_chars);
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
        else {
            DEBUG_printf("Unknown topic %s\n", current_topic);
        }
    }
    DEBUG_printf("Done incoming_data_cb()\n");
}

static cJSON *json_parser(char *data_as_chars)
{
    cJSON *json = cJSON_Parse(data_as_chars);

    if (json == NULL) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL) {
            DEBUG_printf("Error before: %s\n", error_ptr);
        }
        else {
            DEBUG_printf("Unknown JSON parse error\n");
        }
    }
    return json;
}

static message_anim_t message_anim;

static void handle_weather_json_data(char *data_as_chars)
{
    DEBUG_printf("Weather update\n");

    static weather_data_t weather_data;

    cJSON *json = json_parser(data_as_chars);
    if (json == NULL) {
        return;
    }
    else {
        cJSON *inside_temperatures = cJSON_GetObjectItem(json, "inside_temperatures");
        if (!inside_temperatures) {
            DEBUG_printf("Missing inside_temperatures\n");
            cJSON_Delete(json);
            return;
        }

        for (int i = 0; i < cJSON_GetArraySize(inside_temperatures) && i < NO_INSIDE_TEMPERATURES; i++) {
            cJSON *item = cJSON_GetArrayItem(inside_temperatures, i);
            cJSON *name = cJSON_GetObjectItem(item, "name");
            cJSON *temperature = cJSON_GetObjectItem(item, "temperature");

            if (!name || !temperature) {
                DEBUG_printf("Missing field(s) in inside_temperatures\n");
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
            DEBUG_printf("Missing forecasts");
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
                DEBUG_printf("Missing field(s) in forecasts\n");
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
        cJSON *precipitation_probability = cJSON_GetObjectItem(json, "precipitation_probability");
        if (!condition || !temperature || !temperature ||
           !humidity || !wind_speed || !wind_bearing || !wind_bearing || !precipitation_probability)
        {
            DEBUG_printf("Weather has missing field(s)\n");
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
        weather_data.precipitation_probability = cJSON_GetNumberValue(precipitation_probability);

        cJSON_Delete(json);
    }

    message_anim = {
        message_type: MESSAGE_ANIM_WEATHER,
        weather_data: weather_data,
    };
    if (xQueueSend(animate_queue, &message_anim, 10) != pdTRUE) {
        DEBUG_printf("Could not send weather data; dropping\n");
    }

    memset(&weather_data, 0, sizeof(weather_data_t));
}

static void handle_media_player_json_data(char *data_as_chars)
{
    DEBUG_printf("Media player update\n");

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
            DEBUG_printf("Media player update has missing fields\n");
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
        DEBUG_printf("Could not send media_player data; dropping\n");
    }
}

static void handle_calendar_data(char *data_as_chars)
{
    DEBUG_printf("Calendar update\n");

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
                DEBUG_printf("Calendar update has missing fields\n");
                cJSON_Delete(json);
                return;
            }

            strncpy(app->summary, cJSON_GetStringValue(summary), sizeof(app->summary));

            const char *value = cJSON_GetStringValue(start);
            if (strlen(value) < 25) {
                DEBUG_printf("Start datetime is badly formed: %s", value);
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
        DEBUG_printf("Could not send calendar data; dropping\n");
    }
}

static void handle_scroller_json_data(char *data_as_chars)
{
    DEBUG_printf("Scroller update %s\n", data_as_chars);

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
        DEBUG_printf("Not an arrray in scroller data\n");
    }

    cJSON_Delete(json);

    message_anim_t message_anim = {
        message_type: MESSAGE_ANIM_SCROLLER,
        scroller_data: scroller_data,
    };

    if (xQueueSend(animate_queue, &message_anim, 10) != pdTRUE) {
        DEBUG_printf("Could not send scroller data; dropping\n");
    }
}

static void handle_transport_json_data(char *data_as_chars)
{
    DEBUG_printf("Transport update %s\n", data_as_chars);

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
                DEBUG_printf("Transport missing field(s)");
                cJSON_Delete(json);
                return;
            }

            strncpy(transport_data.journies[i].towards, cJSON_GetStringValue(towards), 16);
            strncpy(transport_data.journies[i].departures_summary, cJSON_GetStringValue(departures_summary), 64);
        }

        message_anim_t message_anim = {
            message_type: MESSAGE_ANIM_TRANSPORT,
            transport_data: transport_data,
        };
        DEBUG_printf("Sending transport data\n");
        if (xQueueSend(animate_queue, &message_anim, 10) != pdTRUE) {
            DEBUG_printf("Could not send transport data; dropping\n");
        }
    }
    else {
        DEBUG_printf("Not an arrray in transport data\n");
    }

    cJSON_Delete(json);
}

static void handle_porch_sensor_data(char *data_as_chars)
{
    DEBUG_printf("Porch update: %s\n", data_as_chars);

    porch_t porch = {
        occupied: (strcmp(data_as_chars, "on") == 0) ? true : false,
    };

    message_anim = {
        message_type: MESSAGE_ANIM_PORCH,
        porch: porch,
    };
    if (xQueueSend(animate_queue, &message_anim, 10) != pdTRUE) {
        DEBUG_printf("Could not send media_player data; dropping\n");
    }

    if (porch.occupied) {
        message_buzzer_t message_buzzer = {
            message_type: MESSAGE_BUZZER_SIMPLE,
            simple_type: BUZZER_SIMPLE_PORCH,
        };
        if (xQueueSend(buzzer_queue, &message_buzzer, 10) != pdTRUE) {
            DEBUG_printf("Could not send message_buzzer data; dropping\n");
        }
    }
}

static void handle_notificaiton_data(char *data_as_chars)
{
    DEBUG_printf("Notifation update: %s", data_as_chars);

    cJSON *json = json_parser(data_as_chars);

    if (json == NULL) {
        return;
    }

    if (cJSON_IsObject(json)) {
        cJSON *critical = cJSON_GetObjectItem(json, "critical");
        cJSON *text = cJSON_GetObjectItem(json, "text");
        // RTTTL tune is optional and may be null.
        cJSON *rtttl_tune = cJSON_GetObjectItem(json, "rtttl_tune");

        if (!critical || !text) {
            DEBUG_printf("Notificaiton missing field(s)");
            cJSON_Delete(json);
            return;
        }

        message_anim = {
            message_type: MESSAGE_ANIM_NOTIFICATION,
            notification: {
                critical: (bool) cJSON_IsTrue(critical),
                text: {},
            }
        };

        strncpy(message_anim.notification.text, cJSON_GetStringValue(text), sizeof(message_anim.notification.text));

        if (xQueueSend(animate_queue, &message_anim, 10) != pdTRUE) {
            DEBUG_printf("Could not send anim notification data; dropping");
        }

        message_buzzer_t message_buzzer;

        // Send the optional RTTTL tune, or play a pre-rolled tone sequence.
        if (rtttl_tune) {
            message_buzzer = {
                message_type: MESSAGE_BUZZER_RTTTL,
                rtttl_tune: {},
            };
            strncpy(message_buzzer.rtttl_tune, cJSON_GetStringValue(rtttl_tune),
                sizeof(message_buzzer.rtttl_tune));
        }
        else {
            message_buzzer = {
                message_type: MESSAGE_BUZZER_SIMPLE,
                simple_type: message_anim.notification.critical ?
                    BUZZER_SIMPLE_CRITICAL_NOTIFICATION : BUZZER_SIMPLE_NOTIFICATION,
            };

        }
        if (xQueueSend(buzzer_queue, &message_buzzer, 10) != pdTRUE) {
            DEBUG_printf("Could not send buzzer notification data; dropping\n");
        }
    }
    else {
        DEBUG_printf("Not an object in notification data\n");
    }

    cJSON_Delete(json);
}

static void handle_set_time_data(char *data_as_chars)
{
    DEBUG_printf("handle_set_time_data()\n");

    ds3231_t ds3231;

    bcd_string_to_bytes(data_as_chars, ds3231.datetime_buffer, DS3231_DATETIME_LEN);

    if (xQueueSend(i2c_queue, &ds3231, 10) != pdTRUE) {
        DEBUG_printf("Could not send ds3231 data; dropping\n");
    }
}

static void handle_buzzer_play_rtttl_data(char *data_as_chars)
{
    DEBUG_printf("handle_buzzer_play_rtttl_data()\n");

    message_buzzer_t message_buzzer = {
        .message_type = MESSAGE_BUZZER_RTTTL,
        .rtttl_tune = "",
    };

    strncpy(message_buzzer.rtttl_tune, data_as_chars, sizeof(message_buzzer.rtttl_tune));

    if (xQueueSend(buzzer_queue, &message_buzzer, 10) != pdTRUE) {
        DEBUG_printf("Could not send buzzer data; dropping");
    }
}

static void handle_light_command_data(char *data_as_chars)
{
    message_anim = {
        message_type: MESSAGE_ANIM_BRIGHTNESS,
        brightness: 0,
    };

    if (xQueueSend(animate_queue, &message_anim, 10) != pdTRUE) {
        DEBUG_printf("Could not send light command data; dropping");
    }
}

static void handle_light_brightness_command_data(char *data_as_chars)
{
    DEBUG_printf("Brightness set: %s\n", data_as_chars);

    char *end = NULL;
    int brightness = strtol(data_as_chars, &end, 10);

    if (!end || brightness < 0 || brightness > 255) {
        DEBUG_printf("Brightness out of range or invalid");
        return;
    }

    message_anim = {
        message_type: MESSAGE_ANIM_BRIGHTNESS,
        brightness: (uint8_t) brightness,
    };

    if (xQueueSend(animate_queue, &message_anim, 10) != pdTRUE) {
        DEBUG_printf("Could not send brightness data; dropping");
    }
}

static void handle_set_grayscale_data(char *data_as_chars)
{
    DEBUG_printf("Grayscale update: %s\n", data_as_chars);

    message_anim = {
        message_type: MESSAGE_ANIM_GRAYSCALE,
        grayscale: false,
    };

    if (strcmp(data_as_chars, "ON") == 0) {
        message_anim.grayscale = true;
    } else if (strcmp(data_as_chars, "OFF") == 0) {
        message_anim.grayscale = false;
    } else {
        DEBUG_printf("Malformed grayscale %s\n", data_as_chars);
        return;
    }

    if (xQueueSend(animate_queue, &message_anim, 10) != pdTRUE) {
        DEBUG_printf("Could not send grayscale data; dropping");
    }
}

static void handle_configuration_data(char *attribute, char *data_as_chars)
{
    DEBUG_printf("Configuration update\n");

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
        DEBUG_printf("Could not convert configuration data to number\n");
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
        DEBUG_printf("Unknown configuration attribute: %s", attribute);
    }

    message_anim = {
        message_type: MESSAGE_ANIM_CONFIGURATION,
        configuration: configuration,
    };

    if (xQueueSend(animate_queue, &message_anim, 10) != pdTRUE) {
        DEBUG_printf("Could not send configuration data; dropping\n");
    }
}

bool autodisover_enable = false;
bool send_autodiscover = false;
static void handle_autodiscover_control_data(char *data_as_chars)
{
    DEBUG_printf("AutoDiscover control\n");

    autodisover_enable = strcmp(data_as_chars, "ON") == 0 ? true : false;

    send_autodiscover = true;
}

static void dump_weather_data(weather_data_t *weather_data)
{
    DEBUG_printf("--------------\n");
    DEBUG_printf("Current: condition=%s temperature=%.1f humidity=%.1f\n",
        weather_data->condition, weather_data->temperature, weather_data->humidty);
    for (int i = 0; i < weather_data->forecasts_count; i++) {
        DEBUG_printf("%s: condition=%s temperature=%.1f percipitation_probability=%.1f\n",
            weather_data->forecasts[i].time, weather_data->forecasts[i].condition,
            weather_data->forecasts[i].temperature,
            weather_data->forecasts[i].precipitation_probability);
    }
    DEBUG_printf("--------------\n");
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

#if PICO_SDK    
    cyw43_arch_lwip_begin();
#endif
    int err = mqtt_publish(client, topic, json_chars, strlen(json_chars), 0, 1, mqtt_pub_request_cb, NULL);
    free(json_chars);

#if PICO_SDK
    cyw43_arch_lwip_end();
#endif    
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
                    DEBUG_printf("Got temperature: %s\n", temperature_buffer);

#if BME680_PRESENT
                    snprintf(pressure_buffer, sizeof(pressure_buffer), "%.2f", message_mqtt.climate.pressure);
                    DEBUG_printf("Got pressure: %s\n", pressure_buffer);
                    snprintf(humidity_buffer, sizeof(humidity_buffer), "%.2f", message_mqtt.climate.humidity);
                    DEBUG_printf("Got humidity: %s\n", humidity_buffer);
#endif

#if PICO_SDK
                    cyw43_arch_lwip_begin();
#endif

                    err = mqtt_publish(client, TEMPERATURE_TOPIC, temperature_buffer, strlen(temperature_buffer), 0, 1,
                        mqtt_pub_request_cb, NULL);
                    if (err != ERR_OK) {
                        DEBUG_printf("mqtt_publish on %s return: %d\n", TEMPERATURE_TOPIC, err);
                    }

#if BME680_PRESENT
                    err = mqtt_publish(client, PRESSURE_TOPIC, pressure_buffer, strlen(pressure_buffer), 0, 1,
                        mqtt_pub_request_cb, NULL);
                    if (err != ERR_OK) {
                        DEBUG_printf("mqtt_publish on %s return: %d\n", PRESSURE_TOPIC, err);
                    }
                    err = mqtt_publish(client, HUMIDITY_TOPIC, humidity_buffer, strlen(humidity_buffer), 0, 1,
                        mqtt_pub_request_cb, NULL);
                    if (err != ERR_OK) {
                        DEBUG_printf("mqtt_publish on %s return: %d\n", HUMIDITY_TOPIC, err);
                    }
#endif

#if PICO_SDK
                    cyw43_arch_lwip_end();
#endif
                    break;

                default:
                    panic("Invalid message type (%d)", message_mqtt.message_type);
                    break;
            }
        }

        if (send_autodiscover) {
            int err = 0;
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
#if PICO_SDK
                cyw43_arch_lwip_begin();
#endif

                err += mqtt_publish(client, "homeassistant/light/matrix_display/config", "", 0,
                    0, 1, mqtt_pub_request_cb, NULL) != ERR_OK ? 1 : 0;
                err += mqtt_publish(client, "homeassistant/switch/matrix_display/config", "", 0,
                    0, 1, mqtt_pub_request_cb, NULL) != ERR_OK ? 1 : 0;
                err += mqtt_publish(client, "homeassistant/sensor/matrix_display/config", "", 0,
                    0, 1, mqtt_pub_request_cb, NULL) != ERR_OK ? 1 : 0;
                err += mqtt_publish(client, "homeassistant/number/matrix_display/config", "", 0,
                    0, 1, mqtt_pub_request_cb, NULL) != ERR_OK ? 1 : 0;

#if PICO_SDK
                cyw43_arch_lwip_end();
#endif

            }

            if (err > 0) {
                DEBUG_printf("mqtt_publish returned %d errors\n", err);
            }

            send_autodiscover = false;
        }

        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    else {
        if (mqtt_client_is_connected(client) == 0) {
            DEBUG_printf("MQTT not connected; reconnecting\n");
            do_mqtt_connect(client);
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

static void mqtt_pub_request_cb(void *arg, err_t result)
{
    DEBUG_printf("Publish result: %d\n", result);
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