#include <time.h>

// Messages destined at the animation task

typedef struct {
    char text[64];
    int sequence_number;
} Waiting_t;

typedef struct {
    char time[6];
    char condition[32];
    double temperature;
    double precipitation_probability;
} Forecast_t;

#define NO_FORECASTS 3

typedef struct {
    char name[16];
    double temperature;
} InsideTemperature_t;

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
    Forecast_t forecasts[NO_FORECASTS];
    int inside_temperatures_count;
    InsideTemperature_t inside_temperatures[NO_INSIDE_TEMPERATURES];
} WeatherData_t;

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
} MediaPlayerData_t;

typedef struct {
    char start[16];
    char summary[256];
} Appointment_t;

#define NO_APPOINTMENTS 3

typedef struct {
    Appointment_t appointments[NO_APPOINTMENTS];
} CalendarData_t;

#define NO_SCROLLERS 4

typedef struct {
    int array_size;
    char text[NO_SCROLLERS][256];
} ScrollerData_t;

typedef struct {
    char towards[16];
    char departures_summary[64];
} Journey_t;

#define NO_TRANSPORT_JOURNIES 2

typedef struct {
    Journey_t journies[NO_TRANSPORT_JOURNIES];
} TransportData_t;

typedef struct {
    bool occupied;
} Porch_t;

typedef struct {
    bool critical;
    char text[256];
} Notification_t;

typedef struct {
    float temperature;
    float pressure;
    float humidity;
    uint16_t illuminance;
} Sensor_t;

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
} Configuration_t;

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
        Waiting_t waiting;
        WeatherData_t weather_data;
        MediaPlayerData_t media_player_data;
        CalendarData_t calendar_data;
        ScrollerData_t scroller_data;
        TransportData_t transport_data;
        Porch_t porch;
        Notification_t notification;
        struct tm timeinfo;
        uint8_t brightness;
        bool grayscale;
        Configuration_t configuration;
    };
} MessageAnim_t;

// Messages destined at time task

#define MESSAGE_TIME_NULL 0
#define MESSAGE_TIME_TIMESYNC 1

typedef struct {
    uint8_t message_type;
} MessageTime_t;

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
} MessageBuzzer_t;

// Messages destined at MQTT task

#define MESSAGE_MQTT_NULL 0
#define MESSAGE_MQTT_SENSOR 1

typedef struct {
    uint8_t message_type;
    union {
        Sensor_t sensor;
    };
} MessageMQTT_t;
