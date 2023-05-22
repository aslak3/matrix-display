#include "pico/stdlib.h"

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
    char media_title[128];
    char media_artist[128];
    char media_album_name[128];
} media_player_data_t;

typedef struct {
    char text[256];
} notification_t;

typedef struct {
    bool occupied;
} porch_t;

#define MESSAGE_NULL 0
#define MESSAGE_WEATHER 1
#define MESSAGE_MEDIA_PLAYER 2
#define MESSAGE_NOTIFICATION 3
#define MESSAGE_PORCH 4

typedef struct {
    uint8_t message_type;
    union {
        weather_data_t weather_data;
        media_player_data_t media_player_data;
        notification_t notification;
        porch_t porch;
    };    
} message_t;
