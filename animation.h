#include "pico/stdlib.h"

#include "framebuffer.h"
#include "mqtt.h"

// Incoming data from MQTT

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
    notification_t data;
    int framestamp;
    rgb_t rgb;
    int pixel_length;
} notification_state_t;

typedef struct {
    porch_t data;
    int framestamp;
} porch_state_t;

typedef enum {
    PAGE_WAITING,
    PAGE_CURRENT_WEATHER,
    PAGE_WEATHER_FORECAST,
    PAGE_MEDIA_PLAYER,
} page_t;

typedef struct {
    char message[256];
    int message_pixel_length;
    int message_offset;
    int framestamp;
    int off_countdown;
} scroller_t;

class animation {
    public:
        animation(framebuffer& f);

        void prepare_screen(void);
        void render_page(void);
        void render_notification(void);

        void new_weather_data(weather_data_t *weather_data);
        void new_media_player_data(media_player_data_t *media_player_data);
        void new_notification(notification_t *notification);
        void clear_notification(void);
        void new_porch(porch_t *porch);

    private:
        framebuffer& fb;

        page_t page;
        int frames_left_on_page;

        int frame;
        // When a page was started, which some pages use
        int page_framestamp;

        weather_state_t weather_state;
        media_player_state_t media_player_state;
        notification_state_t notification_state;
        porch_state_t porch_state;

        const rgb_t black = { red: 0, green: 0, blue: 0 };
        const rgb_t white = { red: 0xff, green: 0xff, blue: 0xff };
        const rgb_t red = { red: 0xff, green: 0, blue: 0 };
        const rgb_t green = { red: 0, green: 0xff, blue: 0 };
        const rgb_t blue = { red: 0, green: 0, blue: 0xff };
        const rgb_t cyan = { red: 0, green: 0xff, blue: 0xff };
        const rgb_t yellow = { red: 0xff, green: 0xff, blue: 0 };
        const rgb_t magenta = { red: 0xff, green: 0, blue: 0xff };
        const rgb_t grey = { red: 0x08, green: 0x08, blue: 0x08 };

        font_t *big_font;
        font_t *medium_font;
        font_t *small_font;
        font_t *ibm_font;
        font_t *tiny_font;

        scroller_t scroller;

        void change_page(page_t new_page);

        void render_waiting_page(void);
        void render_current_weather_page(void);
        void render_weather_forecast_page(void);
        bool render_media_player_page(void);
        void update_scroller_message(void);
        void render_scroller(void);

        rgb_t rgb_grey(int grey_level);
};
