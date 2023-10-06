#include <pico/stdlib.h>

typedef struct {
    char time[6];
    char condition[32];
    double temperature;
    double precipitation_probability;
} forecast_t;

#define NO_FORECASTS 8

#define FIELD_WD_CONDITION (1<<0)
#define FIELD_WD_TEMPERATURE (1<<1)
#define FIELD_WD_HUMIDITY (1<<2)
#define FIELD_WD_WIND_SPEED (1<<3)
#define FIELD_WD_WIND_BEARING (1<<4)
#define FIELD_WD_PRESSURE (1<<5)
#define FIELD_WD_FORECAST (1<<6)

#define FIELD_WD_ALL (FIELD_WD_CONDITION | FIELD_WD_TEMPERATURE | FIELD_WD_HUMIDITY | FIELD_WD_FORECAST | \
    FIELD_WD_WIND_SPEED | FIELD_WD_WIND_BEARING | FIELD_WD_PRESSURE)

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
    uint8_t buffer[RTC_DATETIME_LEN];
} rtc_t;

#define MESSAGE_NULL 0
#define MESSAGE_WEATHER 1
#define MESSAGE_MEDIA_PLAYER 2
#define MESSAGE_CALENDAR 3
#define MESSAGE_BLUESTAR 4
#define MESSAGE_NOTIFICATION 10
#define MESSAGE_PORCH 11
#define MESSAGE_RTC 12
#define MESSAGE_BRIGHTNESS 13

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
        uint8_t brightness;
    };    
} message_t;
