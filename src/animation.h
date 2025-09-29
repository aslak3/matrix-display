#include "framebuffer.h"
#include "messages.h"

#define FADE_DURATION 32

// Incoming data from MQTT

typedef struct {
    Waiting_t data;
    int framestamp;
    int pixel_length;
} WaitingState_t;

typedef struct {
    WeatherData_t data;
    int framestamp;
} WeatherState_t;

typedef struct {
    MediaPlayerData_t data;
    int framestamp;
    char message[512];
    int message_pixel_length;
} MediaPlayerState;

typedef struct {
    CalendarData_t data;
    int framestamp;
    int message_pixel_height;
} CalendarState_t;

typedef struct {
    ScrollerData_t data;
    int current_index;
    char message[256];
    int message_pixel_length;
    int message_offset;
    int framestamp;
    int off_countdown;
} ScropllerState_t;

typedef struct {
    TransportData_t data;
    int framestamp;
} TransportState_t;

typedef struct {
    Notification_t data;
    int framestamp;
    rgb_t rgb;
    int pixel_length;
    int repeats;
} NotificationState_t;

typedef struct {
    Porch_t data;
    int framestamp;
} PorchState_t;

typedef struct {
    struct tm data;
    int framestamp;
    int frames_per_second;
    int hide_colons;
    int colon_counter;
    char time_colons[10];
    char time_no_colons[10];
    char date[20];
} TimeState_t;

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
} Page_t;

typedef enum {
    FADE_STATE_IDLE,
    FADE_STATE_OUT,
    FADE_STATE_IN,
} FadeState_t;

#define NO_SNOWFLAKES 255

typedef struct {
    int16_t x, y;
    int16_t weight;
} Snowflake_t;

class Animation {
    public:
        Animation(FrameBuffer& f);

        void prepare_screen(void);
        void render_page(void);
        void render_notification(void);

        void new_waiting(Waiting_t *waiting);
        void new_weather_data(WeatherData_t *weather_data);
        void new_media_player_data(MediaPlayerData_t *media_player_data);
        void new_calendar_data(CalendarData_t *calendar_data);
        void new_scroller_data(ScrollerData_t *scroller_data);
        void new_transport_data(TransportData_t *transport_data);
        void new_notification(Notification_t *notification);
        void clear_notification(void);
        void new_porch(Porch_t *porch);
        void new_time(struct tm *timeinfo);
        void update_configuration(Configuration_t *config);

    private:
        FrameBuffer& fb;

        Page_t page;
        int frames_left_on_page;
        Page_t next_page;
        int fade_countdown;
        FadeState_t fade_state;
        uint8_t fade_brightness;

        int frame;
        // When a page was started, which some pages use
        int page_framestamp;

        WaitingState_t waiting_state;
        WeatherState_t weather_state;
        MediaPlayerState media_player_state;
        CalendarState_t calendar_state;
        ScropllerState_t scroller_state;
        TransportState_t transport_state;
        NotificationState_t notification_state;
        PorchState_t porch_state;
        TimeState_t time_state;

        Snowflake_t snowflakes[NO_SNOWFLAKES];
        int16_t snowflake_wind_vector;

        Configuration_t configuration;

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

        void set_next_page(Page_t new_page);
        void change_page(Page_t new_page);

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
