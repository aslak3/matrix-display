#include "pico/stdlib.h"

#include "framebuffer.h"
#include "mqtt.h"

typedef struct {
    weather_data_t data;
    int framestamp;
} weather_state_t;

typedef struct {
    notification_t data;
    int framestamp;
    rgb_t rgb;
    int pixel_length;
} notification_state_t;

typedef enum {
    PAGE_WEATHER_FORECAST,
} page_t;

class animation {
    public:
        animation(framebuffer& f);

        void prepare_screen(void);
        void render_page(void);
        void render_notification(void);

        void new_weather_data(weather_data_t& weather_data);
        void new_notification(notification_t& notification);
        void clear_notification(void);

    private:
        framebuffer& fb;

        page_t page;
        int frames_left_on_page;

        int frame;

        weather_state_t weather_state;
        notification_state_t notification_state;

        const rgb_t black = { red: 0, green: 0, blue: 0 };
        const rgb_t white = { red: 0xff, green: 0xff, blue: 0xff };
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

        void change_page(page_t new_page);

        void render_weather_forecast(void);

        rgb_t rgb_grey(int grey_level);
};
