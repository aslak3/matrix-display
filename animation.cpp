#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"

#include "animation.h"

animation::animation(framebuffer& f) : fb(f)
{
    big_font = get_font("Noto", 20);
    if (!big_font) panic("Could not find Noto/20");
    medium_font = get_font("Noto", 16);
    if (!medium_font) panic("Could not find Noto/16");
    small_font = get_font("Noto", 10);
    if (!small_font) panic("Could not find Noto/10");
    ibm_font = get_font("IBM", 8);
    if (!ibm_font) panic("Could not find IBM/8");
    tiny_font = get_font("tiny", 6);
    if (!tiny_font) panic("Could not find tiny/8");

    weather_state.framestamp = 0;

    notification_state.framestamp = 0;
    notification_state.rgb = black;
}

void animation::prepare_screen(void)
{
    fb.clear(notification_state.rgb);
}

void animation::render_weather_forecast(int frame)
{
    if (! weather_state.framestamp) {
        return;
    }

    int offset_x = 0;
    for (int forecast_count = 0; forecast_count < 3; forecast_count++) {
        forecast_t& forecast = weather_state.data.forecast[forecast_count];

        fb.printstring(tiny_font, offset_x + 11 - (fb.stringlength(tiny_font, forecast.time) / 2),
            24, forecast.time, white);

        image_t *current_weather_image = get_image(forecast.condition);
        if (!current_weather_image) {
            panic("Could not load current weather condition image for %s",
                forecast.condition);
        }

        fb.showimage(current_weather_image, offset_x + 2, 8);

        char buffer[10];
        memset(buffer, 0, sizeof(buffer));

        if ((frame % 600) < 300) {
            snprintf(buffer, sizeof(buffer), "%dC", (int)forecast.temperature);
            fb.printstring(tiny_font, offset_x + 11 - (fb.stringlength(tiny_font, buffer) / 2), 0, buffer, white);
        }
        else {
            snprintf(buffer, sizeof(buffer), "%d%%", (int)forecast.precipitation_probability);
            fb.printstring(tiny_font, offset_x + 11 - (fb.stringlength(tiny_font, buffer) / 2), 0, buffer, blue);
        }
        offset_x += 22;
    }
}

void animation::render_notification(int frame)
{
    if (! notification_state.framestamp) {
        return;
    }

    notification_state.rgb = rgb_grey(255 - ((frame - notification_state.framestamp) * 16));

    fb.filledbox(0, 8, FB_WIDTH, 16, notification_state.rgb);
    // TODO: Line drawing
    fb.hollowbox(0, 8, FB_WIDTH, 1, white);
    fb.hollowbox(0, 8 + 15, FB_WIDTH, 1, white);
    int notification_offset = (frame - notification_state.framestamp) / 3;
    fb.printstring(medium_font, FB_WIDTH - notification_offset, 8,
        notification_state.data.text, magenta);
    if (notification_offset > notification_state.pixel_length + FB_WIDTH + (FB_WIDTH / 2)) {
        clear_notification();
    }
}

void animation::new_weather_data(int frame, weather_data_t& weather_data)
{
    weather_state.data = weather_data;
    weather_state.framestamp = frame;
}

void animation::new_notification(int frame, notification_t& notification)
{
    notification_state.data = notification;
    notification_state.framestamp = frame;
    notification_state.pixel_length = fb.stringlength(medium_font, notification.text);
    notification_state.rgb = white;
}

void animation::clear_notification(void)
{
    notification_state.framestamp = 0;
}

////

rgb_t animation::rgb_grey(int grey_level)
{
    if (grey_level < 0) {
        return black;
    }
    else if (grey_level > 255) {
        return white;
    }
    else {
        rgb_t rgb = {
            red: (uint8_t) grey_level,
            green: (uint8_t) grey_level,
            blue: (uint8_t) grey_level,
        };
        return rgb;
    }
}