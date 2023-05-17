#include <stdio.h>
#include <math.h>

#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "pico/cyw43_arch.h"

#include "hardware/pio.h"

#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <semphr.h>

#include "framebuffer.hpp"

#include "hub75.pio.h"

#include "mqtt.h"

#define DATA_BASE_PIN 2
#define DATA_N_PINS 6
#define ROWSEL_BASE_PIN 8
#define ROWSEL_N_PINS 4
#define CLK_PIN 13
#define STROBE_PIN 14
#define OEN_PIN 15

void animate_task(void *dummy);
static rgb_t rgb_grey(int grey_level);
void matrix_task(void *dummy);

void vApplicationTickHook(void);

extern void mqtt_task(void *dummy);

/* WIRING
 *
 * R1   GP2
 * B1   GP4
 * R2   GP5
 * B2   GP7
 * A    GP8
 * C    GP10
 * CLK  GP13
 * OE   GP15
 *
 * G1   GP3
 * G2   GP6
 * E    GND
 * B    GP9
 * D    GP11
 * LAT  GP14
 *
 * 0b00000000 00011111 11111111 00000000
 * 0x001fff00
*/

class ball {
    public:
        ball(int x_new, int y_new, int dx_new, int dy_new) {
            x = x_new;
            y = y_new;
            dx = dx_new;
            dy = dy_new;
        }
        void move(void)
        {
            if (x >= FB_WIDTH - 2) {
                dx = -1;
            }
            if (x < 2) {
                dx = 1;
            }
            if (y >= FB_HEIGHT - 2) {
                dy = -1;
            }
            if (y < 2) {
                dy = 1;
            }
            x += dx;
            y += dy;
        }

        int x, y;
        int dx, dy;
};

QueueHandle_t animate_queue;

int main(void)
{
    stdio_init_all();

    printf("Hello, matrix here\n");

    animate_queue = xQueueCreate(10, sizeof(weather_data_t));

    xTaskCreate(&animate_task, "Animate Task", 256, NULL, 0, NULL);
    xTaskCreate(&matrix_task, "Matrix Task", 256, NULL, 10, NULL);
    xTaskCreate(&mqtt_task, "MQTT Task", 1024, NULL, 0, NULL);

    vTaskStartScheduler();

    while(1);

    return 0;
}

framebuffer fb;

const rgb_t black = { red: 0, green: 0, blue: 0 };
const rgb_t white = { red: 0xff, green: 0xff, blue: 0xff };
const rgb_t blue = { red: 0, green: 0, blue: 0xff };
const rgb_t cyan = { red: 0, green: 0xff, blue: 0xff };
const rgb_t yellow = { red: 0xff, green: 0xff, blue: 0 };
const rgb_t magenta = { red: 0xff, green: 0, blue: 0xff };
const rgb_t grey = { red: 0x08, green: 0x08, blue: 0x08 };

void animate_task(void *dummy)
{
#if FREE_RTOS_KERNEL_SMP
    vTaskCoreAffinitySet(NULL, 1 << 0);
    printf("%s: core%u\n", pcTaskGetName(NULL), get_core_num());
#endif

    font_t *big_font = get_font("Noto", 20);
    if (!big_font) panic("Could not find Noto/20");
    font_t *medium_font = get_font("Noto", 16);
    if (!medium_font) panic("Could not find Noto/16");
    font_t *small_font = get_font("Noto", 10);
    if (!small_font) panic("Could not find Noto/10");
    font_t *ibm_font = get_font("IBM", 8);
    if (!ibm_font) panic("Could not find IBM/8");
    font_t *tiny_font = get_font("tiny", 6);
    if (!tiny_font) panic("Could not find tiny/8");

    message_t message;
    memset(&message, 0, sizeof(message_t));

    weather_data_t weather_data;
    memset(&weather_data, 0, sizeof(weather_data_t));
    notification_t notification;
    memset(&notification, 0, sizeof(notification_t));

    int32_t weather_data_framestamp = 0;
    int32_t notification_framestamp = 0;

    for (int32_t frame = 0;; frame++)
    {
        if (xQueueReceive(animate_queue, &message, 0) == pdTRUE) {
            printf("New message, type: %d\n", message.message_type);

            switch (message.message_type) {
                case MESSAGE_WEATHER:
                    weather_data = message.weather_data;
                    weather_data_framestamp = frame;
                    break;

                case MESSAGE_NOTIFICATION:
                    notification = message.notification;
                    notification_framestamp = frame;
                    printf("animate_task: New notification: %s\n", notification.text);
                    break;
                
                default:
                    printf("Unknown message type: %d\n", message.message_type);
            }
        }

        fb.clear();
        rgb_t notification_rgb = black;

        if (notification_framestamp) {
            notification_rgb = rgb_grey(255 - ((frame - notification_framestamp) * 16));
            fb.filledbox(0, 0, FB_WIDTH, FB_HEIGHT, notification_rgb);
        }

        if (weather_data_framestamp) {
            int offset_x = 0;
            for (int forecast_count = 0; forecast_count < 3; forecast_count++) {
                forecast_t *forecast = &weather_data.forecast[forecast_count];
                char buffer[10];
                memset(buffer, 0, sizeof(buffer));

                fb.printstring(tiny_font, offset_x + 11 - (fb.stringlength(tiny_font, forecast->time) / 2),
                    24, forecast->time, white);

                image_t *current_weather_image = get_image(forecast->condition);
                if (!current_weather_image) {
                    panic("Could not load current weather condition image for %s",
                        forecast->condition);
                }

                fb.showimage(current_weather_image, offset_x + 2, 8);

                if ((frame % 600) < 300) {
                    snprintf(buffer, sizeof(buffer), "%dC", (int)forecast->temperature);
                    fb.printstring(tiny_font, offset_x + 11 - (fb.stringlength(tiny_font, buffer) / 2), 0, buffer, white);
                }
                else {
                    snprintf(buffer, sizeof(buffer), "%d%%", (int)forecast->precipitation_probability);
                    fb.printstring(tiny_font, offset_x + 11 - (fb.stringlength(tiny_font, buffer) / 2), 0, buffer, blue);
                }
                offset_x += 22;
            }
        }

        if (notification_framestamp) {
            fb.filledbox(0, 8, FB_WIDTH, 16, notification_rgb);
            // TODO: Line drawing
            fb.hollowbox(0, 8, FB_WIDTH, 1, white);
            fb.hollowbox(0, 8 + 15, FB_WIDTH, 1, white);
            fb.printstring(ibm_font, FB_WIDTH + ((notification_framestamp - frame) / 3), 8, notification.text, magenta);
            if ((frame - notification_framestamp) > 1000) {
                notification_framestamp = 0;
            }
        }

        fb.atomic_back_to_fore_copy();

        vTaskDelay(10);
    }
}

static rgb_t rgb_grey(int grey_level)
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

void matrix_task(void *dummy)
{
#if FREE_RTOS_KERNEL_SMP
    vTaskCoreAffinitySet(NULL, 1 << 1);
    printf("%s: core%u\n", pcTaskGetName(NULL), get_core_num());
#endif

    PIO pio = pio0;
    uint sm_data = 0;
    uint sm_row = 1;

    uint data_prog_offs = pio_add_program(pio, &hub75_data_rgb888_program);
    uint row_prog_offs = pio_add_program(pio, &hub75_row_program);
    hub75_data_rgb888_program_init(pio, sm_data, data_prog_offs, DATA_BASE_PIN, CLK_PIN);
    hub75_row_program_init(pio, sm_row, row_prog_offs, ROWSEL_BASE_PIN, ROWSEL_N_PINS, STROBE_PIN);

    static uint32_t fb_uint32[FB_HEIGHT][FB_WIDTH];

    while (1) {
        fb.atomic_fore_copy_out((rgb_t *) fb_uint32);

        for (int rowsel = 0; rowsel < 16; ++rowsel) {
            for (int bit = 0; bit < 8; ++bit) {
                hub75_data_rgb888_set_shift(pio, sm_data, data_prog_offs, bit);
                for (int x = FB_WIDTH; x >= 0; x--) {
                    pio_sm_put_blocking(pio, sm_data, fb_uint32[rowsel][x]);
                    pio_sm_put_blocking(pio, sm_data, fb_uint32[rowsel + 16][x]);
                }
                // Dummy pixel per lane
                pio_sm_put_blocking(pio, sm_data, 0);
                pio_sm_put_blocking(pio, sm_data, 0);
                // SM is finished when it stalls on empty TX FIFO
                hub75_wait_tx_stall(pio, sm_data);
                // Also check that previous OEn pulse is finished, else things can get out of sequence
                hub75_wait_tx_stall(pio, sm_row);

                // Latch row data, pulse output enable for new row.
                pio_sm_put_blocking(pio, sm_row, rowsel | (100u * (1u << bit) << 5));
            }
        }
    }
}
