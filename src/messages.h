#if PICO_SDK
#include <pico/stdlib.h>
#endif

#include <time.h>

// Messages destined at the animation task

typedef struct {
    char text[64];
    int sequence_number;
} waiting_t;

typedef struct {
    char time[6];
    char condition[32];
    double temperature;
    double precipitation_probability;
} forecast_t;

#define NO_FORECASTS 3

typedef struct {
    char name[16];
    double temperature;
} inside_temperature_t;

#define NO_INSIDE_TEMPERATURES 10

typedef struct {
    int fetched_fields;
    char condition[32];
    double temperature;
    double humidty;
    double wind_speed;
    double wind_bearing;
    double pressure;
    double precipitation_probability;
    int forecasts_count;
    forecast_t forecasts[NO_FORECASTS];
    int inside_temperatures_count;
    inside_temperature_t inside_temperatures[NO_INSIDE_TEMPERATURES];
} weather_data_t;

#define FIELD_MPD_STATE (1<<0)
#define FIELD_MPD_MEDIA_TITLE (1<<2)
#define FIELD_MPD_MEDIA_ARTIST (1<<3)
#define FIELD_MPD_MEDIA_ALBUM_NAME (1<<4)

#define FIELD_MPD_ALL (FIELD_MPD_STATE | FIELD_MPD_MEDIA_TITLE | FIELD_MPD_MEDIA_ARTIST | \
    FIELD_MPD_MEDIA_ALBUM_NAME)

typedef struct {
    int fetched_fields;
    char state[16];
    char media_title[64];
    char media_artist[64];
    char media_album_name[64];
} media_player_data_t;

typedef struct {
    char start[16];
    char summary[256];
} appointment_t;

#define NO_APPOINTMENTS 3

typedef struct {
    appointment_t appointments[NO_APPOINTMENTS];
} calendar_data_t;

#define NO_SCROLLERS 4

typedef struct {
    int array_size;
    char text[NO_SCROLLERS][256];
} scroller_data_t;

typedef struct {
    char towards[16];
    char departures_summary[64];
} journey_t;

#define NO_TRANSPORT_JOURNIES 2

typedef struct {
    journey_t journies[NO_TRANSPORT_JOURNIES];
} transport_data_t;

typedef struct {
    bool occupied;
} porch_t;

typedef struct {
    bool critical;
    char text[256];
} notification_t;

typedef struct {
    float temperature;
    float pressure;
    float humidity;
    uint16_t illuminance;
} sensor_t;

typedef struct {
    int clock_colon_flash;
    int clock_duration;
    int inside_temperatures_scroll_speed;
    int current_weather_duration;
    int weather_forecast_duration;
    int media_player_scroll_speed;
    int calendar_scroll_speed;
    int transport_duration;
    int scroller_interval;
    int scroller_speed;
    int snowflake_count;
} configuration_t;

#define MESSAGE_ANIM_NULL 0
#define MESSAGE_ANIM_WAITING 1
#define MESSAGE_ANIM_WEATHER 2
#define MESSAGE_ANIM_MEDIA_PLAYER 3
#define MESSAGE_ANIM_CALENDAR 4
#define MESSAGE_ANIM_SCROLLER 5
#define MESSAGE_ANIM_NOTIFICATION 6
#define MESSAGE_ANIM_TRANSPORT 7
#define MESSAGE_ANIM_PORCH 8
#define MESSAGE_ANIM_TIMEINFO 9
#define MESSAGE_ANIM_BRIGHTNESS 10
#define MESSAGE_ANIM_GRAYSCALE 11
#define MESSAGE_ANIM_CONFIGURATION 100

typedef struct {
    uint8_t message_type;
    union {
        waiting_t waiting;
        weather_data_t weather_data;
        media_player_data_t media_player_data;
        calendar_data_t calendar_data;
        scroller_data_t scroller_data;
        transport_data_t transport_data;
        porch_t porch;
        notification_t notification;
        struct tm timeinfo;
        uint8_t brightness;
        bool grayscale;
        configuration_t configuration;
    };
} message_anim_t;

// Messages destined at time task

#define MESSAGE_TIME_NULL 0
#define MESSAGE_TIME_TIMESYNC 1

typedef struct {
    uint8_t message_type;
} message_time_t;

// Messages destined at buzzer task

#define MESSAGE_BUZZER_NULL 0
#define MESSAGE_BUZZER_SIMPLE 1
#define MESSAGE_BUZZER_RTTTL 2

#define BUZZER_SIMPLE_NULL                      ((uint8_t)0)
#define BUZZER_SIMPLE_NOTIFICATION              ((uint8_t)1)
#define BUZZER_SIMPLE_CRITICAL_NOTIFICATION     ((uint8_t)2)
#define BUZZER_SIMPLE_PORCH                     ((uint8_t)3)

typedef struct {
    uint8_t message_type;
    union {
        uint8_t simple_type;
        char rtttl_tune[1024];
    };
} message_buzzer_t;

// Messages destined at MQTT task

#define MESSAGE_MQTT_NULL 0
#define MESSAGE_MQTT_SENSOR 1

typedef struct {
    uint8_t message_type;
    union {
        sensor_t sensor;
    };
} message_mqtt_t;
