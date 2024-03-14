#include <pico/stdlib.h>

#include <stdio.h>
#include <string.h>
#include <math.h>

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

    calendar_state.framestamp = 0;

    notification_state.framestamp = 0;
    notification_state.rgb = black;

    porch_state.framestamp = 0;

    configuration.clock_duration = 1000;
    configuration.inside_temperatures_scroll_speed = 3;
    configuration.current_weather_duration = 200;
    configuration.weather_forecast_duration = 500;
    configuration.media_player_scroll_speed = 1;
    configuration.calendar_scroll_speed = 3;
    configuration.bluestar_duration = 300;
    configuration.scroller_interval = 20000;
    configuration.scroller_speed = 2;
    configuration.snowflake_count = 0;

    scroller.off_countdown = configuration.scroller_interval;

    frame = 0;

    change_page(PAGE_WAITING);

    init_snowflakes();
}

void animation::prepare_screen(void)
{
    fb.clear(notification_state.rgb);
}

void animation::render_page(void)
{
    switch (page) {
        case PAGE_WAITING:
            if (render_waiting_page() || ds3231_state.framestamp) {
                change_page(PAGE_RTC);
            }
            break;

        case PAGE_RTC:
            if (!frames_left_on_page || render_clock_page())  {
                change_page(PAGE_INSIDE_TEMPERATURES);
            }
            break;

        case PAGE_INSIDE_TEMPERATURES:
            if (render_inside_temperatures_page())  {
                change_page(PAGE_CURRENT_WEATHER);
            }
            break;

        case PAGE_CURRENT_WEATHER:
            if (!frames_left_on_page || render_current_weather_page()) {
                change_page(PAGE_WEATHER_FORECAST);
            }
            break;

        case PAGE_WEATHER_FORECAST:
            if (!frames_left_on_page || render_weather_forecast_page()) {
                change_page(PAGE_MEDIA_PLAYER);
            }
            break;

        case PAGE_MEDIA_PLAYER:
            if (render_media_player_page()) {
                change_page(PAGE_CALENDAR);
            }
            break;

        case PAGE_CALENDAR:
            if (render_calendar_page()) {
                change_page(PAGE_BLUESTAR);
            }
            break;

        case PAGE_BLUESTAR:
            if (!frames_left_on_page || render_bluestar_page()) {
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

    update_snowflakes();
    render_snowflakes();

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
    weather_state.data = *weather_data;
    weather_state.framestamp = frame;
}

void animation::new_media_player_data(media_player_data_t *media_player_data)
{
    snprintf(media_player_state.message, sizeof(media_player_state.message),
        "'%s' by '%s' on '%s'",
        media_player_data->media_title, media_player_data->media_artist,
        media_player_data->media_album_name);
    media_player_state.message_pixel_length = fb.string_length(medium_font, media_player_state.message);
    media_player_state.data = *media_player_data;
}

void animation::new_calendar_data(calendar_data_t *calendar_data)
{
    printf("Got new calendar data\n");
    calendar_state.data = *calendar_data;
    calendar_state.message_pixel_height = 0;
    calendar_state.framestamp = frame;
}

void animation::new_bluestar_data(bluestar_data_t *bluestar_data)
{
    printf("Got new bluestar data\n");
    bluestar_state.data = *bluestar_data;
    bluestar_state.framestamp = frame;
}

void animation::new_notification(notification_t *notification)
{
    notification_state.data = *notification;
    notification_state.framestamp = frame;
    notification_state.pixel_length = fb.string_length(medium_font, notification->text);
    notification_state.rgb = white;
}

void animation::clear_notification(void)
{
    notification_state.framestamp = 0;
}

void animation::new_porch(porch_t *porch)
{
    porch_state.data = *porch;
    porch_state.framestamp = frame;
}

void animation::new_ds3231(ds3231_t *ds3231)
{
    ds3231_state.data = *ds3231;
    ds3231_state.frames_per_second = frame - ds3231_state.framestamp;
    ds3231_state.framestamp = frame;

    const char *month_names[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
    };
    const char *day_names[] = {
        "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat",
    };

    uint8_t *buffer = ds3231_state.data.datetime_buffer;

    snprintf(ds3231_state.time, sizeof(ds3231_state.time), "%02x:%02x:%02x",
        buffer[2] & 0x3f,
        buffer[1] & 0x7f,
        buffer[0] & 0x7f
    );

    const int day_number = (buffer[3] & 0x0f) + (((buffer[3] & 0xf0) >> 4) * 10) - 1;
    const int month_number = (buffer[5] & 0x0f) + (((buffer[5] & 0xf0) >> 4) * 10) - 1;

    if (month_number < 12 && day_number < 7) {
        snprintf(ds3231_state.date, sizeof(ds3231_state.date), "%s %02x %s",
            day_names[day_number],
            buffer[4],
            month_names[month_number]
        );
    }
    else {
        strncpy(ds3231_state.date, "XXXX", sizeof(ds3231_state.date));
    }

    printf("New Time: %s Date: %s\n", ds3231_state.time, ds3231_state.date);
}

void animation::update_configuration(configuration_t *config)
{
    if (config->clock_duration >= 0) {
        configuration.clock_duration = config->clock_duration;
    }
    if (config->inside_temperatures_scroll_speed >= 0) {
        configuration.inside_temperatures_scroll_speed = config->inside_temperatures_scroll_speed;
    }
    if (config->current_weather_duration >= 0) {
        configuration.current_weather_duration = config->current_weather_duration;
    }
    if (config->weather_forecast_duration >= 0) {
        configuration.weather_forecast_duration = config->weather_forecast_duration;
    }
    if (config->media_player_scroll_speed >= 0) {
        configuration.media_player_scroll_speed = config->media_player_scroll_speed;
    }
    if (config->calendar_scroll_speed > 0) {
        configuration.calendar_scroll_speed = config->calendar_scroll_speed;
    }
    if (config->bluestar_duration >= 0) {
        configuration.bluestar_duration = config->bluestar_duration;
    }
    if (config->scroller_interval >= 0) {
        configuration.scroller_interval = config->scroller_interval;
    }
    if (config->scroller_speed > 0) {
        configuration.scroller_speed = config->scroller_speed;
    }
    if (config->snowflake_count >= 0) {
        configuration.snowflake_count = config->snowflake_count;
    }
}

////

void animation::change_page(page_t new_page)
{
    printf("changing to new page %d\n");
    page_framestamp = frame;

    page = new_page;

    switch (new_page) {
        case PAGE_WAITING:
            break;

        case PAGE_RTC:
            frames_left_on_page = configuration.clock_duration;
            break;

        case PAGE_INSIDE_TEMPERATURES:
            frames_left_on_page = 0;
            break;

        case PAGE_CURRENT_WEATHER:
            frames_left_on_page = configuration.current_weather_duration;
            break;

        case PAGE_WEATHER_FORECAST:
            frames_left_on_page = configuration.weather_forecast_duration;
            break;

        case PAGE_MEDIA_PLAYER:
            frames_left_on_page = 0;
            media_player_state.framestamp = frame;
            break;

        case PAGE_CALENDAR:
            frames_left_on_page = 0;
            calendar_state.framestamp = frame;
            break;

        case PAGE_BLUESTAR:
            frames_left_on_page = configuration.bluestar_duration;
            break;

        default:
            panic("Don't know how to switch page to %d\n", new_page);
            break;
    }
}

bool animation::render_waiting_page(void)
{
    fb.print_string(big_font, 6, 4, "Wait...", white);

    return false;
}

bool animation::render_clock_page(void)
{
    fb.print_string(ibm_font, 0, (FB_HEIGHT - 1) - 8 - 6, ds3231_state.time, orange);

    fb.print_string(tiny_font, (FB_WIDTH / 2) - (fb.string_length(tiny_font, ds3231_state.date) / 2),
        (FB_HEIGHT - 1) - 8 - 10 - 6, ds3231_state.date, white);

    return false;
}

bool animation::render_inside_temperatures_page(void)
{
    if (!(weather_state.framestamp && weather_state.data.inside_temperatures_count)) return true;

    int running_y = 0 - tiny_font->height + ((frame - page_framestamp) / configuration.inside_temperatures_scroll_speed);

    for (int c = 0; c < weather_state.data.inside_temperatures_count; c++) {
        inside_temperature_t *inside_temp = &weather_state.data.inside_temperatures[c];

        if (!strlen(inside_temp->name)) {
            continue;
        }

        fb.print_string(tiny_font, 0, running_y, inside_temp->name, orange);
        // running_y -= tiny_font->height + 1;

        char buf[10];
        snprintf(buf, sizeof(buf), "%.1fC", inside_temp->temperature);
        fb.print_string(tiny_font, 42, running_y, buf, yellow);
        running_y -= tiny_font->height + 1;
        running_y -= 3;
    }

    if (running_y >= FB_HEIGHT) {
        return true;
    }
    else {
        return false;
    }
}

bool animation::render_current_weather_page(void)
{
    if (! weather_state.framestamp) return true;

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

    return false;
}

bool animation::render_weather_forecast_page(void)
{
    if (! weather_state.framestamp) return true;

    int offset_x = 0;
    for (int c = 0; c < 3; c++) {
        forecast_t& forecast = weather_state.data.forecasts[c];

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
            fb.print_string(tiny_font, offset_x + 11 - (fb.string_length(tiny_font, buffer) / 2), 16, buffer, light_blue);
        }

        image_t *current_weather_image = get_image(forecast.condition, 16, 16);
        if (!current_weather_image) {
            panic("Could not load current weather condition image for %s",
                forecast.condition);
        }

        fb.show_image(current_weather_image, offset_x + 2, 0);

        offset_x += 22;
    }

    return false;
}

// true = go to next page
bool animation::render_media_player_page(void)
{
    media_player_data_t *mpd = &media_player_state.data;

    if ((strcmp(mpd->state, "playing") != 0) && strcmp(mpd->state, "paused") != 0) {
        return true;
    }

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
        int message_offset = (frame - media_player_state.framestamp) / configuration.media_player_scroll_speed;
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
    if (!(calendar_state.framestamp)) return true;

    int running_y = 0 - tiny_font->height + ((frame - page_framestamp) / configuration.calendar_scroll_speed);

    for (int c = 0; c < NO_APPOINTMENTS; c++) {
        appointment_t *app = &calendar_state.data.appointments[c];

        if (!strlen(app->start) || !strlen(app->summary)) {
            continue;
        }

        fb.print_string(tiny_font, 0, running_y, app->start, yellow);
        running_y -= tiny_font->height + 1;
        running_y = fb.print_wrapped_string(tiny_font, running_y, app->summary, white);
        running_y -= 3;
    }

    if (running_y >= FB_HEIGHT) {
        return true;
    }
    else {
        return false;
    }
}

bool animation::render_bluestar_page(void)
{
    if (!(bluestar_state.framestamp)) return true;

    if (!(strlen(bluestar_state.data.journies[0].towards)) && strlen(bluestar_state.data.journies[1].towards)) {
        return true;
    }

    rgb_t towards_colour = light_blue;
    rgb_t departures_colour = cyan;
    // If bus info is stale (older then 5 minutes) make it red instead of blue
    if (frame - ds3231_state.framestamp > 300 * ds3231_state.frames_per_second) {
        towards_colour = dark_red;
        departures_colour = red;
    }

    fb.print_string(tiny_font, 0, 24, bluestar_state.data.journies[0].towards,
        towards_colour);
    fb.print_string(tiny_font, 0, 16, bluestar_state.data.journies[0].departures_summary,
        departures_colour);

    fb.print_string(tiny_font, 0, 8, bluestar_state.data.journies[1].towards,
        towards_colour);
    fb.print_string(tiny_font, 0, 0, bluestar_state.data.journies[1].departures_summary,
        departures_colour);

    return false;
}

void animation::update_scroller_message(void)
{
    weather_data_t *wd = &weather_state.data;
    snprintf(scroller.message, sizeof(scroller.message),
        "CURRENTLY... TEMP: %d C; HUMIDTY: %d %%; PRESSURE: %d hPa; WIND: %d km/h FROM %d \x7f",
        (int) wd->temperature, (int) wd->humidty, (int) wd->pressure, (int) wd->wind_speed,
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

        scroller.message_offset = (frame - scroller.framestamp) / configuration.scroller_speed;
        fb.print_string(tiny_font, FB_WIDTH - scroller.message_offset, 0,
            scroller.message, light_blue);

        if (scroller.message_offset >
            scroller.message_pixel_length + FB_WIDTH + (FB_WIDTH / 2))
        {
            scroller.off_countdown = configuration.scroller_interval;
        }
    }
}

int16_t animation::random_snowflake_x(void)
{
    return rand() % (128 * 256) - (64 * 256);
}

void animation::init_snowflakes(void)
{
    snowflake_wind_vector = 0;

    for (int c = 0; c < NO_SNOWFLAKES; c++) {
        snowflakes[c].weight = rand() % 64 + 8;
        snowflakes[c].x = random_snowflake_x();
        snowflakes[c].y = rand() % (32 * 256);
    }
}

#define MAX_WIND_SPEED 64
void animation::update_snowflakes(void)
{
    snowflake_wind_vector += (rand() % 32) - 16;
    if (snowflake_wind_vector > MAX_WIND_SPEED) {
        snowflake_wind_vector = MAX_WIND_SPEED;
    }
    if (snowflake_wind_vector < -MAX_WIND_SPEED) {
        snowflake_wind_vector = -MAX_WIND_SPEED;
    }

    for (int c = 0; c < configuration.snowflake_count && c < NO_SNOWFLAKES; c++) {
        snowflakes[c].x += snowflake_wind_vector;
        snowflakes[c].y -= snowflakes[c].weight;
        if (snowflakes[c].y <= 0) {
            snowflakes[c].x = random_snowflake_x();
            snowflakes[c].y = 31 * 256;
        }
    }
}

void animation::render_snowflakes(void)
{
    for (int c = 0; c < configuration.snowflake_count && c < NO_SNOWFLAKES; c++) {
        fb.point((snowflakes[c].x + (32 * 256)) / 256, snowflakes[c].y / 256, bright_white);
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
