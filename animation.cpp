#include <pico/stdlib.h>

#include <stdio.h>
#include <string.h>

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

    porch_state.framestamp = 0;

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
            if (rtc_state.framestamp) {
                change_page(PAGE_RTC);
            }
            break;

        case PAGE_RTC:
            render_rtc_page();
            if (! frames_left_on_page && weather_state.framestamp) {
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
                if ((strcmp(media_player_state.data.state, "playing") == 0) ||
                    (strcmp(media_player_state.data.state, "paused") == 0))
                {
                    change_page(PAGE_MEDIA_PLAYER);
                }
                else if (calendar_state.framestamp) {
                    change_page(PAGE_CALENDAR);
                }
                else {
                    change_page(PAGE_RTC);
                }
            }
            break;

        case PAGE_MEDIA_PLAYER:
            if (render_media_player_page()) {
                if (calendar_state.framestamp) {
                    change_page(PAGE_CALENDAR);
                }
                else {
                    change_page(PAGE_RTC);
                }
            }
            break;

        case PAGE_CALENDAR:
            if (render_calendar_page()) {
                change_page(PAGE_RTC);
            }
            break;

        default:
            panic("Asked to render non existant page %d", page);
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
    if (porch_state.framestamp) {
        if (porch_state.data.occupied == true && frame & 0x40) {
            image_t *porch_image = get_image("porch", 21, 8);
            if (! porch_image) {
                panic("Could not find porch image");
            }

            fb.show_image(porch_image, 0, FB_HEIGHT - 1 - 8, 255, false);
        }
    }

    if (! notification_state.framestamp) {
        return;
    }

    notification_state.rgb = rgb_grey(255 - ((frame - notification_state.framestamp) * 16));

    fb.filled_box(0, 8, FB_WIDTH, 16, notification_state.rgb);
    // TODO: Line drawing
    fb.hollow_box(0, 8, FB_WIDTH, 1, white);
    fb.hollow_box(0, 8 + 15, FB_WIDTH, 1, white);
    int notification_offset = (frame - notification_state.framestamp) / 2;
    fb.print_string(medium_font, FB_WIDTH - notification_offset, 8,
        notification_state.data.text, magenta);
    if (notification_offset > notification_state.pixel_length + FB_WIDTH + (FB_WIDTH / 2)) {
        clear_notification();
    }
}

void animation::new_weather_data(weather_data_t *weather_data)
{
    // printf("new_weather_data()\n");
    weather_state.data = *weather_data;
    weather_state.framestamp = frame;
    // printf("End of new_weather_data()\n");
}

void animation::new_media_player_data(media_player_data_t *media_player_data)
{
    // printf("new_media_player_data()\n");
    snprintf(media_player_state.message, sizeof(media_player_state.message),
        "'%s' by '%s' on '%s'",
        media_player_data->media_title, media_player_data->media_artist,
        media_player_data->media_album_name);
    media_player_state.message_pixel_length = fb.string_length(medium_font, media_player_state.message);
    media_player_state.data = *media_player_data;
    // printf("End of new_media_player_data()\n");
}

void animation::new_calendar_data(calendar_data_t *calendar_data)
{
    calendar_state.data = *calendar_data;
    calendar_state.message_pixel_height = 0;
}

void animation::new_notification(notification_t *notification)
{
    // printf("new_notification()\n");
    notification_state.data = *notification;
    notification_state.framestamp = frame;
    notification_state.pixel_length = fb.string_length(medium_font, notification->text);
    notification_state.rgb = white;
}

void animation::clear_notification(void)
{
    // printf("clear_notification()\n");
    notification_state.framestamp = 0;
}

void animation::new_porch(porch_t *porch)
{
    // printf("new_porch()\n");
    porch_state.data = *porch;
    porch_state.framestamp = frame;
}

void animation::new_rtc(rtc_t *rtc)
{
    // printf("new_rtc()\n");
    rtc_state.data = *rtc;
    rtc_state.framestamp = frame;
}

////

void animation::change_page(page_t new_page)
{
    // printf("changing to new page %d\n");
    page_framestamp = frame;

    page = new_page;

    switch (new_page) {
        case PAGE_WAITING:
            break;

        case PAGE_RTC:
            frames_left_on_page = 1000;
            break;

        case PAGE_CURRENT_WEATHER:
            frames_left_on_page = 500;
            break;

        case PAGE_WEATHER_FORECAST:
            frames_left_on_page = 1000;
            break;

        case PAGE_MEDIA_PLAYER:
            frames_left_on_page = 0;
            media_player_state.framestamp = frame;
            break;

        case PAGE_CALENDAR:
            frames_left_on_page = 0;
            calendar_state.framestamp = frame;
            break;

        default:
            panic("Don't know how to switch page to %d\n", new_page);
            break;
    }
}

void animation::render_waiting_page(void)
{
    fb.print_string(big_font, 6, 4, "Wait...", white);
}

void animation::render_rtc_page(void)
{
    const char *month_names[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
    };
    const char *day_names[] = {
        "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat",
    };

    uint8_t *buffer = rtc_state.data.buffer;

    // Month field is crazy!! Flatten out the 4th bit of the BCD horrror.
    if (buffer[5] == 0x10) {
        buffer[5] = 0x09;
    } else if (buffer[5] == 0x11) {
        buffer[5] = 0x0a;
    } else if (buffer[5] == 0x12) {
        buffer[5] = 0x0b;
    }

    char time[10];
    snprintf(time, sizeof(time), "%02x:%02x:%02x",
        buffer[2] & 0x3f,
        buffer[1] & 0x7f,
        buffer[0] & 0x7f
    );
    fb.print_string(ibm_font, 0, FB_HEIGHT - 8 - 1 - 6, time, orange);

    char date[20];

    if (buffer[4] <= 6 && (buffer[5] & 0x0f) <= 11) {
        snprintf(date, sizeof(date), "%s %02x %s",
            day_names[buffer[4]],
            buffer[3],
            month_names[buffer[5] & 0x0f]
        );
    }
    else {
        strncpy(date, "XXXX", sizeof(date));
    }
    fb.print_string(tiny_font, (FB_WIDTH / 2) - (fb.string_length(tiny_font, date) / 2),
        FB_HEIGHT - 8 - 10 - 6, date, white);
}

void animation::render_current_weather_page(void)
{
    image_t *current_weather_image = get_image(weather_state.data.condition, 32, 32);
    if (!current_weather_image) {
        panic("Could not load current weather condition image for %s %dx%d",
            weather_state.data.condition);
    }

    fb.show_image(current_weather_image, 4, 0);

    char buffer[10];
    memset(buffer, 0, sizeof(buffer));

    snprintf(buffer, sizeof(buffer), "%dC", (int)weather_state.data.temperature);
    fb.print_string(ibm_font, 40, 20, buffer, yellow);
}

void animation::render_weather_forecast_page(void)
{
    int offset_x = 0;
    for (int forecast_count = 0; forecast_count < 3; forecast_count++) {
        forecast_t& forecast = weather_state.data.forecast[forecast_count];

        fb.print_string(tiny_font, offset_x + 11 - (fb.string_length(tiny_font, forecast.time) / 2),
            24, forecast.time, white);

        char buffer[10];
        memset(buffer, 0, sizeof(buffer));

        if ((frame % 600) < 300) {
            snprintf(buffer, sizeof(buffer), "%dC", (int)forecast.temperature);
            fb.print_string(tiny_font, offset_x + 11 - (fb.string_length(tiny_font, buffer) / 2), 16, buffer, yellow);
        }
        else {
            snprintf(buffer, sizeof(buffer), "%d%%", (int)forecast.precipitation_probability);
            fb.print_string(tiny_font, offset_x + 11 - (fb.string_length(tiny_font, buffer) / 2), 16, buffer, blue);
        }

        image_t *current_weather_image = get_image(forecast.condition, 16, 16);
        if (!current_weather_image) {
            panic("Could not load current weather condition image for %s",
                forecast.condition);
        }

        fb.show_image(current_weather_image, offset_x + 2, 0);

        offset_x += 22;
    }
}

// true = go to next page
bool animation::render_media_player_page(void)
{
    media_player_data_t *mpd = &media_player_state.data;

    const char *state_image_name = "mp-off";
    if (strcmp(mpd->state, "playing") == 0) {
        state_image_name = "mp-play";
    }
    else if (strcmp(mpd->state, "paused") == 0) {
        state_image_name = "mp-pause";
    }
    image_t *state_image = get_image(state_image_name, 32, 32);
    if (! state_image) {
        panic("Could not load media_player state image for %s", mpd->state);
    }
    fb.show_image(state_image, 16, 0, 0x40, true);

    if (strcmp(mpd->state, "off") != 0 ) {
        int message_offset = frame - media_player_state.framestamp;
        fb.print_string(medium_font, FB_WIDTH - message_offset, 8,
            media_player_state.message, cyan);

        if (message_offset >
            media_player_state.message_pixel_length + FB_WIDTH + (FB_WIDTH / 2))
        {
            return true;
        }
    }
    return false;
}

// true = go to next page
bool animation::render_calendar_page(void)
{
    int running_y = 0 - tiny_font->height + ((frame - page_framestamp) / 3);

    for (int appointment_index = 0; appointment_index < NO_APPOINTMENTS; appointment_index++) {
        appointment_t *app = &calendar_state.data.appointments[appointment_index];

        if (!strlen(app->start) || !strlen(app->summary)) {
            continue;
        }

        fb.print_string(tiny_font, 0, running_y, app->start, yellow);
        running_y -= tiny_font->height + 1;
        running_y = fb.print_wrapped_string(tiny_font, running_y, app->summary, white);
        running_y -= 3;
    }

    if (running_y > FB_HEIGHT) {
        return true;
    }
    else {
        return false;
    }

}

void animation::update_scroller_message(void)
{
    weather_data_t *wd = &weather_state.data;
    snprintf(scroller.message, sizeof(scroller.message),
        "HUMIDTY: %d %%; PRESSURE: %d hPa; WIND: %d km/h FROM %d \x7f",
        (int) wd->humidty, (int) wd->pressure, (int) wd->wind_speed,
        (int) wd->wind_bearing);

    scroller.message_pixel_length = fb.string_length(tiny_font,
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
        fb.shadow_box(0, 0, FB_WIDTH, 8, 0x10);

        scroller.message_offset = (frame - scroller.framestamp) / 2;
        fb.print_string(tiny_font, FB_WIDTH - scroller.message_offset, 0,
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