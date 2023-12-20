#include <pico/stdlib.h>

// Messages destined at the animation task

typedef struct {
    char time[6];
    char condition[32];
    double temperature;
    double precipitation_probability;
} forecast_t;

#define NO_FORECASTS 3

typedef struct {
    int fetched_fields;
    char condition[32];
    double temperature;
    double humidty;
    double wind_speed;
    double wind_bearing;
    double pressure;
    double precipitation_probability;
    int forecast_count;
    forecast_t forecast[NO_FORECASTS];
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
    char summary[128];
} appointment_t;

#define NO_APPOINTMENTS 3

typedef struct {
    appointment_t appointments[NO_APPOINTMENTS];
} calendar_data_t;

typedef struct {
    char towards[16];
    char departures_summary[64];
} journey_t;

#define NO_BUS_JOURNIES 2

typedef struct {
    journey_t journies[NO_BUS_JOURNIES];
} bluestar_data_t;

typedef struct {
    char text[256];
} notification_t;

typedef struct {
    bool occupied;
} porch_t;

#define RTC_DATETIME_LEN 7

typedef struct {
    uint8_t datetime_buffer[RTC_DATETIME_LEN];
} rtc_t;

#define BRIGHTNESS_UNKNWON 0
#define BRIGHTNESS_RED 1
#define BRIGHTNESS_GREEN 2
#define BRIGHTNESS_BLUE 3

typedef struct {
    uint8_t type;
    uint8_t intensity;
} brightness_t;

typedef struct {
    int rtc_duration;
    int current_weather_duration;
    int weather_forecast_duration;
    int media_player_scroll_speed;
    int calendar_scroll_speed;
    int bluestar_duration;
    int scroller_interval;
    int scroller_speed;
    int snowflake_count;
} configuration_t;

#define MESSAGE_ANIM_NULL 0
#define MESSAGE_ANIM_WEATHER 1
#define MESSAGE_ANIM_MEDIA_PLAYER 2
#define MESSAGE_ANIM_CALENDAR 3
#define MESSAGE_ANIM_BLUESTAR 4
#define MESSAGE_ANIM_NOTIFICATION 10
#define MESSAGE_ANIM_PORCH 11
#define MESSAGE_ANIM_RTC 12
#define MESSAGE_ANIM_BRIGHTNESS 13
#define MESSAGE_ANIM_GRAYSCALE 14
#define MESSAGE_ANIM_CONFIGURATION 100

typedef struct {
    uint8_t message_type;
    union {
        weather_data_t weather_data;
        media_player_data_t media_player_data;
        calendar_data_t calendar_data;
        bluestar_data_t bluestar_data;
        notification_t notification;
        porch_t porch;
        rtc_t rtc;
        brightness_t brightness;
        bool grayscale;
        configuration_t configuration;
    };
} message_anim_t;

// Messages destined at RTC task

#define MESSAGE_RTC_NULL 0
#define MESSAGE_RTC_RTC 1

typedef struct {
    uint8_t message_type;
    union {
        rtc_t rtc;
    };
} message_rtc_t;

// Messages destined at MQTT task

#define MESSAGE_MQTT_NULL 0
#define MESSAGE_MQTT_TEMPERATURE 1

typedef struct {
    uint8_t message_type;
    union {
        double temperature;
    };
} message_mqtt_t;
