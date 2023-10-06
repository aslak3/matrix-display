#include <stdio.h>
#include <math.h>

#include <pico/stdlib.h>
#include <hardware/gpio.h>
#include <hardware/pio.h>
#include <pico/cyw43_arch.h>

#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <semphr.h>

#include "animation.h"

#include "hub75.pio.h"

#define DATA_BASE_PIN 2
#define DATA_N_PINS 6
#define ROWSEL_BASE_PIN 8
#define ROWSEL_N_PINS 4
#define CLK_PIN 13
#define STROBE_PIN 14
#define OEN_PIN 15

void animate_task(void *dummy);
void matrix_task(void *dummy);
void rtc_task(void *dummy);

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

QueueHandle_t animate_queue;
QueueHandle_t rtc_queue;
QueueHandle_t matrix_queue;

int main(void)
{
    stdio_init_all();

    printf("Hello, matrix here\n");

    animate_queue = xQueueCreate(3, sizeof(message_t));
    rtc_queue = xQueueCreate(3, sizeof(rtc_t));
    matrix_queue = xQueueCreate(1, sizeof(fb_t));

    xTaskCreate(&animate_task, "Animate Task", 4096, NULL, 0, NULL);
    xTaskCreate(&matrix_task, "Matrix Task", 1024, NULL, 10, NULL);
    xTaskCreate(&mqtt_task, "MQTT Task", 4096, NULL, 0, NULL);
    xTaskCreate(&rtc_task, "RTC Task", 4096, NULL, 0, NULL);

    vTaskStartScheduler();

    while(1);

    return 0;
}

framebuffer fb;

void animate_task(void *dummy)
{
#if FREE_RTOS_KERNEL_SMP
    vTaskCoreAffinitySet(NULL, 1 << 0);
    printf("%s: core%u\n", pcTaskGetName(NULL), get_core_num());
#endif

    message_t message;
    memset(&message, 0, sizeof(message_t));

    animation anim(fb);

    while (1)
    {
        while (xQueueReceive(animate_queue, &message, 0) == pdTRUE) {
            // printf("New message, type: %d\n", message.message_type);

            switch (message.message_type) {
                case MESSAGE_WEATHER:
                    anim.new_weather_data(&message.weather_data);
                    break;

                case MESSAGE_MEDIA_PLAYER:
                    anim.new_media_player_data(&message.media_player_data);
                    break;

                case MESSAGE_CALENDAR:
                    anim.new_calendar_data(&message.calendar_data);
                    break;

                case MESSAGE_BLUESTAR:
                    anim.new_bluestar_data(&message.bluestar_data);
                    break;

                case MESSAGE_NOTIFICATION:
                    anim.new_notification(&message.notification);
                    break;
                
                case MESSAGE_PORCH:
                    anim.new_porch(&message.porch);
                    break;

                case MESSAGE_RTC:
                    anim.new_rtc(&message.rtc);
                    break;

                case MESSAGE_BRIGHTNESS:
                    fb.set_brightness(message.brightness);

                default:
                    printf("Unknown message type: %d\n", message.message_type);
                    break;
            }
        }

        anim.prepare_screen();

        anim.render_page();

        anim.render_notification();

        fb.atomic_back_to_fore_copy();

        vTaskDelay(10);
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

    static fb_t output_fb;

    while (1) {
        fb.atomic_fore_copy_out(&output_fb);

        for (int rowsel = 0; rowsel < 16; ++rowsel) {
            for (int bit = 0; bit < 8; ++bit) {
                hub75_data_rgb888_set_shift(pio, sm_data, data_prog_offs, bit);
                for (int x = 0; x < FB_WIDTH; x++) {
                    pio_sm_put_blocking(pio, sm_data, output_fb.uint32[rowsel][x]);
                    pio_sm_put_blocking(pio, sm_data, output_fb.uint32[rowsel + 16][x]);
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
