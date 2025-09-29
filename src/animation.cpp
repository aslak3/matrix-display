#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "animation.h"

Animation::Animation(FrameBuffer& f) : fb(f)
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

    configuration.clock_colon_flash = 0;
    configuration.clock_duration = 1000;
    configuration.inside_temperatures_scroll_speed = 3;
    configuration.current_weather_duration = 200;
    configuration.weather_forecast_duration = 500;
    configuration.media_player_scroll_speed = 1;
    configuration.calendar_scroll_speed = 3;
    configuration.transport_duration = 300;
    configuration.scroller_interval = 10000;
    configuration.scroller_speed = 2;
    configuration.snowflake_count = 0;

    waiting_state.framestamp = 0;

    time_state.colon_counter = 0;
    time_state.hide_colons = 0;
    time_state.framestamp = 0;

    weather_state.framestamp = 0;

    calendar_state.framestamp = 0;

    scroller_state.current_index = 0;
    scroller_state.off_countdown = configuration.scroller_interval;

    notification_state.framestamp = 0;
    notification_state.rgb = black;

    porch_state.framestamp = 0;

    frame = 0;

    change_page(PAGE_WAITING);

    fade_state = FADE_STATE_IDLE;
    fade_countdown = 0;
    fade_brightness = 255;

    init_snowflakes();
}

void Animation::prepare_screen(void)
{
    fb.clear(notification_state.rgb);
}

void Animation::render_page(void)
{
    uint8_t brightness = (uint8_t)((fade_countdown * 255) / FADE_DURATION);

    if (fade_state == FADE_STATE_IDLE) {
        fade_brightness = 255;
    }
    else if (fade_state == FADE_STATE_OUT) {
        fade_countdown--;
        if (fade_countdown == 0) {
            change_page(next_page);
            fade_state = FADE_STATE_IN;
            fade_countdown = FADE_DURATION;
        }
        fade_brightness = brightness;
    }
    else if (fade_state == FADE_STATE_IN) {
        fade_countdown--;
        if (fade_countdown == 0) {
            fade_state = FADE_STATE_IDLE;
        }
        fade_brightness = 255 - brightness;
    }

    switch (page) {
        case PAGE_WAITING:
            // Only come out of waiting if we have time from SNTP *and* MQTT is connected
            if (render_waiting_page()) {
                set_next_page(PAGE_RTC);
            }
            break;

        case PAGE_RTC:
            if (render_clock_page() || !frames_left_on_page)  {
                set_next_page(PAGE_INSIDE_TEMPERATURES);
            }
            break;

        case PAGE_INSIDE_TEMPERATURES:
            if (render_inside_temperatures_page())  {
                set_next_page(PAGE_CURRENT_WEATHER);
            }
            break;

        case PAGE_CURRENT_WEATHER:
            if (render_current_weather_page() || !frames_left_on_page) {
                set_next_page(PAGE_WEATHER_FORECAST);
            }
            break;

        case PAGE_WEATHER_FORECAST:
            if (render_weather_forecast_page() || !frames_left_on_page) {
                set_next_page(PAGE_MEDIA_PLAYER);
            }
            break;

        case PAGE_MEDIA_PLAYER:
            if (render_media_player_page()) {
                set_next_page(PAGE_CALENDAR);
            }
            break;

        case PAGE_CALENDAR:
            if (render_calendar_page()) {
                set_next_page(PAGE_TRANSPORT);
            }
            break;

        case PAGE_TRANSPORT:
            if (render_transport_page() || !frames_left_on_page) {
                set_next_page(PAGE_RTC);
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

void Animation::set_next_page(Page_t p)
{
    if (fade_state == FADE_STATE_IDLE) {
        DEBUG_printf("Setting the next page to %d\n", p);
        next_page = p;
        fade_state = FADE_STATE_OUT;
        fade_countdown = FADE_DURATION;
    }
}

void Animation::render_notification(void)
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

    if (notification_state.data.critical) {
        int pulse = (frame - notification_state.framestamp) % 32;
        if (pulse > 15) {
            pulse = 32 - pulse;
        }

        notification_state.rgb.red = pulse * 8;
        notification_state.rgb.green = 0;
        notification_state.rgb.blue = 0;
        // Draw the background again, removing all foreground.
        fb.clear(notification_state.rgb);
    }
    else {
        notification_state.rgb = rgb_grey(255 - ((frame - notification_state.framestamp) * 16));
    }

    fb.filled_box(0, 8, FB_WIDTH, 16, black);
    fb.line(0, 8, FB_WIDTH, 8, white);
    fb.line(0, 8 + 16, FB_WIDTH, 8 + 16, white);

    int notification_offset = (frame - notification_state.framestamp) / 2;
    fb.print_string(medium_font, FB_WIDTH - notification_offset, 8,
        notification_state.data.text, notification_state.data.critical ? red : yellow);
    if (notification_offset > notification_state.pixel_length + FB_WIDTH + (FB_WIDTH / 2)) {
        notification_state.repeats--;
        if (notification_state.repeats == 0) {
            clear_notification();
        }
        else {
            notification_state.framestamp = frame;
        }
    }
}

void Animation::new_waiting(Waiting_t *waiting)
{
    waiting_state.data = *waiting;
    waiting_state.framestamp = frame;
    waiting_state.pixel_length = fb.string_length(medium_font, waiting->text);
}

void Animation::new_weather_data(WeatherData_t *weather_data)
{
    DEBUG_printf("Got new weather data\n");
    weather_state.data = *weather_data;
    weather_state.framestamp = frame;
}

void Animation::new_media_player_data(MediaPlayerData_t *media_player_data)
{
    DEBUG_printf("Got new media player data\n");
    snprintf(media_player_state.message, sizeof(media_player_state.message),
        "'%s' by '%s' on '%s'",
        media_player_data->media_title, media_player_data->media_artist,
        media_player_data->media_album_name);
    media_player_state.message_pixel_length = fb.string_length(medium_font, media_player_state.message);
    media_player_state.data = *media_player_data;
}

void Animation::new_calendar_data(CalendarData_t *calendar_data)
{
    DEBUG_printf("Got new calendar data\n");
    calendar_state.data = *calendar_data;
    calendar_state.message_pixel_height = 0;
    calendar_state.framestamp = frame;
}

void Animation::new_scroller_data(ScrollerData_t *scroller_data)
{
    DEBUG_printf("Got new scroller data\n");
    scroller_state.data = *scroller_data;
    for (int i = 0; i < scroller_state.data.array_size; i++) {
        special_replacement(scroller_state.data.text[i], "-UP-", CHAR_UP_ARROW);
        special_replacement(scroller_state.data.text[i], "-DOWN-", CHAR_DOWN_ARROW);
    }
}

void Animation::new_transport_data(TransportData_t *transport_data)
{
    DEBUG_printf("Got new transport data\n");
    transport_state.data = *transport_data;
    transport_state.framestamp = frame;
}

void Animation::new_notification(Notification_t *notification)
{
    DEBUG_printf("Got new notification data\n");
    notification_state.data = *notification;
    notification_state.framestamp = frame;
    notification_state.pixel_length = fb.string_length(medium_font, notification->text);
    notification_state.rgb = white;
    notification_state.repeats = notification_state.data.critical ? 3 : 1;
}

void Animation::clear_notification(void)
{
    notification_state.rgb = black;
    notification_state.framestamp = 0;
}

void Animation::new_porch(Porch_t *porch)
{
    porch_state.data = *porch;
    porch_state.framestamp = frame;
}

void Animation::new_time(struct tm *timeinfo)
{
    time_state.data = *timeinfo;
    time_state.frames_per_second = frame - time_state.framestamp;
    time_state.framestamp = frame;

    const char *month_names[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
    };
    const char *day_names[] = {
        "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat",
    };

    snprintf(time_state.time_colons, sizeof(time_state.time_colons), "%02d:%02d:%02d",
        time_state.data.tm_hour,
        time_state.data.tm_min,
        time_state.data.tm_sec
    );
    snprintf(time_state.time_no_colons, sizeof(time_state.time_no_colons), "%02d %02d %02d",
        time_state.data.tm_hour,
        time_state.data.tm_min,
        time_state.data.tm_sec
    );

    snprintf(time_state.date, sizeof(time_state.date), "%s %d %s",
        day_names[time_state.data.tm_wday],
        time_state.data.tm_mday,
        month_names[time_state.data.tm_mon]
    );

    DEBUG_printf("New Time: %s Date: %s\n", time_state.time_colons, time_state.date);
}

void Animation::update_configuration(Configuration_t *config)
{
    if (config->clock_colon_flash >= 0) {
        configuration.clock_colon_flash = config->clock_colon_flash;
    }
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
    if (config->transport_duration >= 0) {
        configuration.transport_duration = config->transport_duration;
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

void Animation::change_page(Page_t new_page)
{
    DEBUG_printf("Changing to page %d\n", new_page);
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

        case PAGE_TRANSPORT:
            frames_left_on_page = configuration.transport_duration;
            break;

        default:
            panic("Don't know how to switch page to %d\n", new_page);
            break;
    }
}

bool Animation::render_waiting_page(void)
{
    rgb_t colours[] = {
        blue, red, magenta, green, cyan, yellow, white
    };

    // If we haven't got a waiting progress message from the MQTT task yet then just do nothing
    if (!waiting_state.framestamp) {
        return false;
    }

    // Start scrolling 25 frames later
    int waiting_offset = 2 * (frame - (waiting_state.framestamp + 25));
    if (waiting_offset < 0) {
        waiting_offset = 0;
    }
    // When we get to the end of scrolling hold the start of the message on the screen
    if (waiting_offset > waiting_state.pixel_length + FB_WIDTH) {
        waiting_offset = 0;
    }

    rgb_t colour = white;

    // The last message is always white
    if (waiting_state.data.sequence_number >= 0) {
        colour = colours[waiting_state.data.sequence_number % (sizeof(colours) / sizeof(rgb_t))];
    }

    fb.print_string(medium_font, 0 - waiting_offset, 8, waiting_state.data.text, colour);

    // Conditions for leaving waiting: we have the time, we have the last waiting message
    // from the MQTT task, and at least 100 frames have passed since then
    if (
        time_state.framestamp &&
        (waiting_state.data.sequence_number < 0) &&
        (frame - waiting_state.framestamp > 100)
    ) {
        return true;
    }
    else {
        return false;
    }
}

bool Animation::render_clock_page(void)
{
    fb.print_string(ibm_font, 0, (FB_HEIGHT - 1) - 8 - 6,
        time_state.hide_colons ? time_state.time_no_colons : time_state.time_colons,
        rgb_faded(orange));

    fb.print_string(tiny_font, (FB_WIDTH / 2) - (fb.string_length(tiny_font, time_state.date) / 2),
        (FB_HEIGHT - 1) - 8 - 10 - 6, time_state.date, rgb_faded(white));

    if (configuration.clock_colon_flash) {
        time_state.colon_counter++;
        if (time_state.colon_counter > configuration.clock_colon_flash) {
            time_state.hide_colons = !time_state.hide_colons;
            time_state.colon_counter = 0;
        }
    }
    else {
        // Not flashing? Force colons to shown.
        time_state.hide_colons = 0;
    }

    return false;
}

bool Animation::render_inside_temperatures_page(void)
{
    if (!(weather_state.framestamp && weather_state.data.inside_temperatures_count)) return true;

    int running_y = 0 - tiny_font->height + ((frame - page_framestamp) / configuration.inside_temperatures_scroll_speed);

    for (int c = 0; c < weather_state.data.inside_temperatures_count; c++) {
        InsideTemperature_t *inside_temp = &weather_state.data.inside_temperatures[c];

        if (!strlen(inside_temp->name)) {
            continue;
        }

        fb.print_string(tiny_font, 0, running_y, inside_temp->name, rgb_faded(orange));

        char buf[10];
        snprintf(buf, sizeof(buf), "%.1fC", inside_temp->temperature);
        fb.print_string(tiny_font, 42, running_y, buf, rgb_faded(yellow));
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

bool Animation::render_current_weather_page(void)
{
    if (! weather_state.framestamp) return true;

    image_t *current_weather_image = get_image(weather_state.data.condition, 32, 32);
    if (!current_weather_image) {
        panic("Could not load current weather condition image for %s %dx%d",
            weather_state.data.condition);
    }

    fb.show_image(current_weather_image, 4, 0, fade_brightness, false);

    char buffer[10];
    memset(buffer, 0, sizeof(buffer));

    snprintf(buffer, sizeof(buffer), "%dC", (int)weather_state.data.temperature);
    fb.print_string(ibm_font, 50 - (fb.string_length(ibm_font, buffer) / 2), 20, buffer, rgb_faded(yellow));

    snprintf(buffer, sizeof(buffer), "%d%%", (int)weather_state.data.precipitation_probability);
    fb.print_string(ibm_font, 50 - (fb.string_length(ibm_font, buffer) / 2), 4, buffer, rgb_faded(light_blue));

    return false;
}

bool Animation::render_weather_forecast_page(void)
{
    if (! weather_state.framestamp) return true;

    int offset_x = 0;
    for (int c = 0; c < 3; c++) {
        Forecast_t& forecast = weather_state.data.forecasts[c];

        fb.print_string(tiny_font, offset_x + 11 - (fb.string_length(tiny_font, forecast.time) / 2),
            24, forecast.time, rgb_faded(white));

        char buffer[10];
        memset(buffer, 0, sizeof(buffer));

        if ((frame % 400) < 200) {
            snprintf(buffer, sizeof(buffer), "%dC", (int)forecast.temperature);
            fb.print_string(tiny_font, offset_x + 11 - (fb.string_length(tiny_font, buffer) / 2), 16, buffer, rgb_faded(yellow));
        }
        else {
            snprintf(buffer, sizeof(buffer), "%d%%", (int)forecast.precipitation_probability);
            fb.print_string(tiny_font, offset_x + 11 - (fb.string_length(tiny_font, buffer) / 2), 16, buffer, rgb_faded(light_blue));
        }

        image_t *current_weather_image = get_image(forecast.condition, 16, 16);
        if (!current_weather_image) {
            panic("Could not load current weather condition image for %s",
                forecast.condition);
        }

        fb.show_image(current_weather_image, offset_x + 2, 0, fade_brightness, false);

        offset_x += 22;
    }

    return false;
}

// true = go to next page
bool Animation::render_media_player_page(void)
{
    MediaPlayerData_t *mpd = &media_player_state.data;

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
    fb.show_image(state_image, 16, 0, (64 * fade_brightness) / 255, true);

    if (strcmp(mpd->state, "off") != 0 ) {
        int message_offset = (frame - media_player_state.framestamp) / configuration.media_player_scroll_speed;
        fb.print_string(medium_font, FB_WIDTH - message_offset, 8,
            media_player_state.message, rgb_faded(cyan));

        if (message_offset >
            media_player_state.message_pixel_length + FB_WIDTH + (FB_WIDTH / 2))
        {
            return true;
        }
    }
    return false;
}

// true = go to next page
bool Animation::render_calendar_page(void)
{
    if (!(calendar_state.framestamp)) return true;

    int running_y = 0 - tiny_font->height + ((frame - page_framestamp) / configuration.calendar_scroll_speed);

    for (int c = 0; c < NO_APPOINTMENTS; c++) {
        Appointment_t *app = &calendar_state.data.appointments[c];

        if (!strlen(app->start) || !strlen(app->summary)) {
            continue;
        }

        fb.print_string(tiny_font, 0, running_y, app->start, rgb_faded(yellow));
        running_y -= tiny_font->height + 1;
        running_y = fb.print_wrapped_string(tiny_font, running_y, app->summary, rgb_faded(white));
        running_y -= 3;
    }

    if (running_y >= FB_HEIGHT) {
        return true;
    }
    else {
        return false;
    }
}

bool Animation::render_transport_page(void)
{
    if (!(transport_state.framestamp)) return true;

    if (!(strlen(transport_state.data.journies[0].towards)) && strlen(transport_state.data.journies[1].towards)) {
        return true;
    }

    rgb_t towards_colour = rgb_faded(light_blue);
    rgb_t departures_colour = rgb_faded(cyan);
    // If bus info is stale (older then 5 minutes) make it red instead of blue
    if (frame - time_state.framestamp > 300 * time_state.frames_per_second) {
        towards_colour = rgb_faded(dark_red);
        departures_colour = rgb_faded(red);
    }

    fb.print_string(tiny_font, 0, 24, transport_state.data.journies[0].towards,
        towards_colour);
    fb.print_string(tiny_font, 0, 16, transport_state.data.journies[0].departures_summary,
        departures_colour);

    fb.print_string(tiny_font, 0, 8, transport_state.data.journies[1].towards,
        towards_colour);
    fb.print_string(tiny_font, 0, 0, transport_state.data.journies[1].departures_summary,
        departures_colour);

    return false;
}

void Animation::update_scroller_message(void)
{
    strncpy(scroller_state.message, scroller_state.data.text[scroller_state.current_index], 256);
    scroller_state.message_pixel_length = fb.string_length(tiny_font,
        scroller_state.message
    );

    DEBUG_printf("update_scroller_message: message is: %s\n", scroller_state.message);

    scroller_state.message_offset = 0;
    scroller_state.framestamp = frame;

    scroller_state.current_index++;
    if (!(scroller_state.current_index < scroller_state.data.array_size)) {
        scroller_state.current_index = 0;
    }
}

void Animation::render_scroller(void)
{
    if (scroller_state.data.array_size == 0) {
        return;
    }

    if (scroller_state.off_countdown) {
        scroller_state.off_countdown--;
        if (! scroller_state.off_countdown) {
            update_scroller_message();
        }
    }
    else {
        fb.shadow_box(0, 0, FB_WIDTH, 8, 0x10);

        scroller_state.message_offset = (frame - scroller_state.framestamp) / configuration.scroller_speed;
        fb.print_string(tiny_font, FB_WIDTH - scroller_state.message_offset, 0,
            scroller_state.message, light_blue);

        if (scroller_state.message_offset >
            scroller_state.message_pixel_length + FB_WIDTH + (FB_WIDTH / 2))
        {
            scroller_state.off_countdown = configuration.scroller_interval;
        }
    }
}

int16_t Animation::random_snowflake_x(void)
{
    return rand() % (128 * 256) - (64 * 256);
}

void Animation::init_snowflakes(void)
{
    snowflake_wind_vector = 0;

    for (int c = 0; c < NO_SNOWFLAKES; c++) {
        snowflakes[c].weight = rand() % 64 + 8;
        snowflakes[c].x = random_snowflake_x();
        snowflakes[c].y = rand() % (32 * 256);
    }
}

#define MAX_WIND_SPEED 64
void Animation::update_snowflakes(void)
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

void Animation::render_snowflakes(void)
{
    for (int c = 0; c < configuration.snowflake_count && c < NO_SNOWFLAKES; c++) {
        fb.set_pixel((snowflakes[c].x + (32 * 256)) / 256, snowflakes[c].y / 256, bright_white);
    }
}

rgb_t Animation::rgb_grey(int grey_level)
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
            flags: (uint8_t) 0,
        };
        return rgb;
    }
}

rgb_t Animation::rgb_faded(rgb_t rgb)
{
    return rgb_t {
        red: (uint8_t)((uint32_t)(rgb.red * fade_brightness) / 256),
        green: (uint8_t)((uint32_t)(rgb.green * fade_brightness) / 256),
        blue: (uint8_t)((uint32_t)(rgb.blue * fade_brightness) / 256),
        flags: (uint8_t) 0,
    };
}
