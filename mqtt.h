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
#define FIELD_FORECAST (1<<3)

typedef struct {
    int fetched_fields;
    char condition[CONDITION_LEN];
    double temperature;
    double humidty;
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
