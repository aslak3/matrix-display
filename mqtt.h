#include "pico/stdlib.h"

#define CONDITION_LEN 32

typedef struct {
    char time[6];
    char condition[CONDITION_LEN];
    double temperature;
    double precipitation_probability;
} forecast_t;

#define NO_FORECASTS 8

#define FIELD_CONDITION (1<<0)
#define FIELD_TEMPERATURE (1<<1)
#define FIELD_HUMIDITY (1<<2)
#define FIELD_WIND_SPEED (1<<3)
#define FIELD_WIND_BEARING (1<<4)
#define FIELD_PRESSURE (1<<5)
#define FIELD_FORECAST (1<<6)

#define FIELD_ALL (FIELD_CONDITION | FIELD_TEMPERATURE | FIELD_HUMIDITY | FIELD_FORECAST | \
    FIELD_WIND_SPEED | FIELD_WIND_BEARING | FIELD_PRESSURE)

typedef struct {
    int fetched_fields;
    char condition[CONDITION_LEN];
    double temperature;
    double humidty;
    double wind_speed;
    double wind_bearing;
    double pressure;
    double precipitation_probability;
    int forecast_count;
    forecast_t forecast[NO_FORECASTS];
} weather_data_t;

#define NOTIFICATION_LENGTH 256

typedef struct {
    char text[NOTIFICATION_LENGTH];
} notification_t;

#define MESSAGE_NULL 0
#define MESSAGE_WEATHER 1
#define MESSAGE_NOTIFICATION 2

typedef struct {
    uint8_t message_type;
    union {
        weather_data_t weather_data;
        notification_t notification;
    };    
} message_t;
