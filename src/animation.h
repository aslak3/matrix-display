#if PICO_SDK
#include <pico/stdlib.h>
#endif

#include "framebuffer.h"
#include "messages.h"

#define FADE_DURATION 32

// Incoming data from MQTT

typedef struct {
    waiting_t data;
    int framestamp;
    int pixel_length;
} waiting_state_t;

typedef struct {
    weather_data_t data;
    int framestamp;
} weather_state_t;

typedef struct {
    media_player_data_t data;
    int framestamp;
    char message[512];
    int message_pixel_length;
} media_player_state_t;

typedef struct {
    calendar_data_t data;
    int framestamp;
    int message_pixel_height;
} calendar_state_t;

typedef struct {
    scroller_data_t data;
    int current_index;
    char message[256];
    int message_pixel_length;
    int message_offset;
    int framestamp;
    int off_countdown;
} scroller_state_t;

typedef struct {
    transport_data_t data;
    int framestamp;
} transport_state_t;

typedef struct {
    notification_t data;
    int framestamp;
    rgb_t rgb;
    int pixel_length;
    int repeats;
} notification_state_t;

typedef struct {
    porch_t data;
    int framestamp;
} porch_state_t;

typedef struct {
    struct tm data;
    int framestamp;
    int frames_per_second;
    int hide_colons;
    int colon_counter;
    char time_colons[10];
    char time_no_colons[10];
    char date[20];
} time_state_t;

typedef enum {
    PAGE_NULL,
    PAGE_WAITING,
    PAGE_RTC,
    PAGE_INSIDE_TEMPERATURES,
    PAGE_CURRENT_WEATHER,
    PAGE_WEATHER_FORECAST,
    PAGE_MEDIA_PLAYER,
    PAGE_CALENDAR,
    PAGE_TRANSPORT,
} page_t;

typedef enum {
    FADE_STATE_IDLE,
    FADE_STATE_OUT,
    FADE_STATE_IN,
} fade_state_t;

#define NO_SNOWFLAKES 255

typedef struct {
    int16_t x, y;
    int16_t weight;
} snowflake_t;

class animation {
    public:
        animation(framebuffer& f);

        void prepare_screen(void);
        void render_page(void);
        void render_notification(void);

        void new_waiting(waiting_t *waiting);
        void new_weather_data(weather_data_t *weather_data);
        void new_media_player_data(media_player_data_t *media_player_data);
        void new_calendar_data(calendar_data_t *calendar_data);
        void new_scroller_data(scroller_data_t *scroller_data);
        void new_transport_data(transport_data_t *transport_data);
        void new_notification(notification_t *notification);
        void clear_notification(void);
        void new_porch(porch_t *porch);
        void new_time(struct tm *timeinfo);
        void update_configuration(configuration_t *config);

    private:
        framebuffer& fb;

        page_t page;
        int frames_left_on_page;
        page_t next_page;
        int fade_countdown;
        fade_state_t fade_state;
        uint8_t fade_brightness;

        int frame;
        // When a page was started, which some pages use
        int page_framestamp;

        waiting_state_t waiting_state;
        weather_state_t weather_state;
        media_player_state_t media_player_state;
        calendar_state_t calendar_state;
        scroller_state_t scroller_state;
        transport_state_t transport_state;
        notification_state_t notification_state;
        porch_state_t porch_state;
        time_state_t time_state;

        snowflake_t snowflakes[NO_SNOWFLAKES];
        int16_t snowflake_wind_vector;

        configuration_t configuration;

        const rgb_t black = { red: 0, green: 0, blue: 0, flags: 0 };
        const rgb_t white = { red: 0xff, green: 0xff, blue: 0xff, flags: 0 };
        const rgb_t red = { red: 0xff, green: 0, blue: 0, flags: 0 };
        const rgb_t dark_red = { red: 0x80, green: 0, blue: 0, flags: 0 };
        const rgb_t green = { red: 0, green: 0xff, blue: 0, flags: 0 };
        const rgb_t dark_green { red: 0, green: 0x80, blue: 0, flags: 0 };
        const rgb_t blue = { red: 0, green: 0, blue: 0xff, flags: 0 };
        const rgb_t dark_blue = { red: 0, green: 0, blue: 0x80, flags: 0 };
        const rgb_t cyan = { red: 0, green: 0xff, blue: 0xff, flags: 0 };
        const rgb_t yellow = { red: 0xff, green: 0xff, blue: 0, flags: 0 };
        const rgb_t magenta = { red: 0xff, green: 0, blue: 0xff, flags: 0 };
        const rgb_t grey = { red: 0x80, green: 0x80, blue: 0x80, flags: 0 };
        const rgb_t orange { red: 0xff, green: 0x80, blue: 0, flags: 0 };
        const rgb_t light_blue = { red: 0, green: 0x40, blue: 0xff, flags: 0 };

        const rgb_t bright_white = { red: 0xff, green: 0xff, blue: 0xff, flags: RGB_FLAGS_BRIGHT };

        font_t *big_font;
        font_t *medium_font;
        font_t *small_font;
        font_t *ibm_font;
        font_t *tiny_font;

        void set_next_page(page_t new_page);
        void change_page(page_t new_page);

        bool render_waiting_page(void);
        bool render_clock_page(void);
        bool render_inside_temperatures_page(void);
        bool render_current_weather_page(void);
        bool render_weather_forecast_page(void);
        bool render_media_player_page(void);
        bool render_calendar_page(void);
        bool render_transport_page(void);

        void update_scroller_message(void);
        void render_scroller(void);

        int16_t random_snowflake_x(void);
        void init_snowflakes(void);
        void update_snowflakes(void);
        void render_snowflakes(void);

        rgb_t rgb_grey(int grey_level);
        rgb_t rgb_faded(rgb_t rgb);
};
