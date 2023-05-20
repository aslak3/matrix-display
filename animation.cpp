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

    scroller.off_countdown = 1000;

    frame = 0;

    change_page(PAGE_WAITING);
}


void animation::prepare_screen(void)
{
    fb.clear(notification_state.rgb);
}

void animation::render_page(void)
{
    switch (page) {
        case PAGE_WAITING:
            render_waiting_page();
            if (weather_state.framestamp) {
                change_page(PAGE_CURRENT_WEATHER);
            }
            break;

        case PAGE_CURRENT_WEATHER:
            render_current_weather_page();
            if (! frames_left_on_page) {
                change_page(PAGE_WEATHER_FORECAST);
            }
            break;

        case PAGE_WEATHER_FORECAST:
            render_weather_forecast_page();
            if (! frames_left_on_page) {
                printf("frames left is zero %s\n", media_player_state.data.state);
                if (strcmp(media_player_state.data.state, "off") != 0) {
                    printf("switching to media player\n");
                    change_page(PAGE_MEDIA_PLAYER);
                }
                else {
                    printf("switching to weather\n");
                    change_page(PAGE_CURRENT_WEATHER);
                }
            }
            break;

        case PAGE_MEDIA_PLAYER:
            if (render_media_player_page()) {
                change_page(PAGE_CURRENT_WEATHER);
            }
            break;

        default:
            break;
    }

    if (page != PAGE_WAITING) {
        render_scroller();
    }

    if (frames_left_on_page) {
        frames_left_on_page--;
    }
    frame++;
}

void animation::render_notification(void)
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

void animation::new_weather_data(weather_data_t& weather_data)
{
        weather_state.data = weather_data;
    weather_state.framestamp = frame;
}

void animation::new_media_player_data(media_player_data_t& media_player_data)
{
    snprintf(media_player_state.message, sizeof(media_player_state.message),
        "'%s' by '%s' on '%s'",
        media_player_data.media_title, media_player_data.media_artist,
        media_player_data.media_album_name);
    media_player_state.message_pixel_length = fb.stringlength(medium_font, media_player_state.message);
    media_player_state.data = media_player_data;
}

void animation::new_notification(notification_t& notification)
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

void animation::change_page(page_t new_page)
{
    page_framestamp = frame;

    page = new_page;

    switch (new_page) {
        case PAGE_WAITING:
            break;

        case PAGE_CURRENT_WEATHER:
            frames_left_on_page = 1000;
            break;

        case PAGE_WEATHER_FORECAST:
            frames_left_on_page = 1000;
            break;

        case PAGE_MEDIA_PLAYER:
            frames_left_on_page = 0;
            media_player_state.framestamp = frame;
            break;

        default:
            panic("Don't know how to switch page to %d\n", new_page);
            break;
    }
}

void animation::render_waiting_page(void)
{
    fb.printstring(big_font, 6, 4, "Wait...", white);
}

void animation::render_current_weather_page(void)
{
    image_t *current_weather_image = get_image(weather_state.data.condition, 28, 28);
    if (!current_weather_image) {
        panic("Could not load current weather condition image for %s %dx%d",
            weather_state.data.condition);
    }

    fb.showimage(current_weather_image, 2, 2, 0x40);

    char buffer[10];
    memset(buffer, 0, sizeof(buffer));

    snprintf(buffer, sizeof(buffer), "T:%dC", (int)weather_state.data.temperature);
    fb.printstring(ibm_font, 24, 22, buffer, yellow);

    snprintf(buffer, sizeof(buffer), "H:%d%%", (int)weather_state.data.humidty);
    fb.printstring(ibm_font, 24, 12, buffer, green);
}

void animation::render_weather_forecast_page(void)
{
    int offset_x = 0;
    for (int forecast_count = 0; forecast_count < 3; forecast_count++) {
        forecast_t& forecast = weather_state.data.forecast[forecast_count];

        fb.printstring(tiny_font, offset_x + 11 - (fb.stringlength(tiny_font, forecast.time) / 2),
            24, forecast.time, white);

        char buffer[10];
        memset(buffer, 0, sizeof(buffer));

        if ((frame % 600) < 300) {
            snprintf(buffer, sizeof(buffer), "%dC", (int)forecast.temperature);
            fb.printstring(tiny_font, offset_x + 11 - (fb.stringlength(tiny_font, buffer) / 2), 16, buffer, yellow);
        }
        else {
            snprintf(buffer, sizeof(buffer), "%d%%", (int)forecast.precipitation_probability);
            fb.printstring(tiny_font, offset_x + 11 - (fb.stringlength(tiny_font, buffer) / 2), 16, buffer, blue);
        }

        image_t *current_weather_image = get_image(forecast.condition, 16, 16);
        if (!current_weather_image) {
            panic("Could not load current weather condition image for %s",
                forecast.condition);
        }

        fb.showimage(current_weather_image, offset_x + 2, 0);

        offset_x += 22;
    }
}

// true = go to next page
bool animation::render_media_player_page(void)
{
    media_player_data_t *mpd = &media_player_state.data;

    fb.printstring(ibm_font, (FB_WIDTH / 2) - (fb.stringlength(ibm_font, mpd->state) / 2),
        21, mpd->state, blue);

    if (strcmp(mpd->state, "off") != 0 ) {
        int message_offset = (frame - media_player_state.framestamp) / 2;
        fb.printstring(medium_font, FB_WIDTH - message_offset, 8,
            media_player_state.message, cyan);

        if (message_offset >
            media_player_state.message_pixel_length + FB_WIDTH + (FB_WIDTH / 2))
        {
            return true;
        }
    }
    return false;
}

void animation::update_scroller_message(void)
{
    weather_data_t *wd = &weather_state.data;
    snprintf(scroller.message, sizeof(scroller.message),
        "PRESSURE: %d hPa; WIND: %d km/h FROM %d",
        (int) wd->pressure, (int) wd->wind_speed, (int) wd->wind_bearing);

    scroller.message_pixel_length = fb.stringlength(tiny_font,
        scroller.message);
    scroller.message_offset = 0;
    scroller.framestamp = frame;
}

void animation::render_scroller(void)
{
    if (scroller.off_countdown) {
        scroller.off_countdown--;
        if (! scroller.off_countdown) {
            update_scroller_message();
            scroller.framestamp = frame;
        }
    }
    else {
        fb.shadowbox(0, 0, FB_WIDTH, 8, 0x10);

        scroller.message_offset = (frame - scroller.framestamp) / 3;
        fb.printstring(tiny_font, FB_WIDTH - scroller.message_offset, 0,
            scroller.message, blue);

        if (scroller.message_offset >
            scroller.message_pixel_length + FB_WIDTH + (FB_WIDTH / 2))
        {
            scroller.off_countdown = 4000;
        }
    }
}

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