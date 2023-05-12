#include <stdio.h>

#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "pico/cyw43_arch.h"

#include "hardware/pio.h"
#include "hub75.pio.h"


#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>

#include "framebuffer.hpp"

#define DATA_BASE_PIN 2
#define DATA_N_PINS 6
#define ROWSEL_BASE_PIN 8
#define ROWSEL_N_PINS 4
#define CLK_PIN 13
#define STROBE_PIN 14
#define OEN_PIN 15

void animate_task(void *dummy);
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

SemaphoreHandle_t xSemaphore = NULL;

int main(void)
{
    stdio_init_all();

    printf("Hello, matrix here\n");

    xSemaphore = xSemaphoreCreateMutex();

    xTaskCreate(&animate_task, "Animate Task", 256, NULL, 0, NULL);
    xTaskCreate(&matrix_task, "Matrix Task", 256, NULL, 10, NULL);
    xTaskCreate(&mqtt_task, "MQTT Task", 1024, NULL, 0, NULL);

    vTaskStartScheduler();

    while(1);

    return 0;
}

framebuffer fb;

char topic_message[256] = "TESTING";

inline void framebuffer::swap(void)
{
    rgb_t (*temp)[FB_HEIGHT][FB_WIDTH];

    temp = foreground_rgb;
    background_rgb = foreground_rgb;
    foreground_rgb = temp;
}

void animate_task(void *dummy)
{
#if FREE_RTOS_KERNEL_SMP
    vTaskCoreAffinitySet(NULL, 1 << 0);
    printf("%s: core%u\n", pcTaskGetName(NULL), get_core_num());
#endif

    ball ball1(3, 3, 1, 1);
    ball ball2(10, 10, -1, -1);
    ball ball3(24, 10, -1, 1);

    for (uint32_t frame = 0;; frame++)
    {
        fb.clear();
        fb.showimage();
        fb.printstring(2, 16, topic_message,
            (rgb_t){ .red = 0x40, .green = 0, .blue = 0xff });
        fb.printstring(22, 0, "Test",
            (rgb_t){ .red = 0x20, .green = 0x40, .blue = 0x20 });

        fb.filledbox(ball1.x - 1, ball1.y - 1, 3, 3,
            (rgb_t) { .red = 0, .green = 0xff, .blue = 0x80 });
        fb.hollowbox(ball2.x - 1, ball2.y - 1, 3, 3,
            (rgb_t) { .red = 0xff, .green = 0xff, .blue = 0 });
        fb.filledbox(ball3.x - 1, ball3.y - 1, 3, 3,
            (rgb_t) { .red = 0xff, .green = 0x80, .blue = 0xff });

        // if (xSemaphoreTake(xSemaphore, (TickType_t) 0) == pdTRUE) {
            taskENTER_CRITICAL();
            fb.swap();
            taskEXIT_CRITICAL();
        //     xSemaphoreGive(xSemaphore);
        // }

        if ((frame & 0x1) == 0) {
            ball1.move();
            ball2.move();
        }
        if ((frame & 0x3) == 0) {
            ball3.move();
        }

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

    static uint32_t gc_row[2][FB_WIDTH];

    while (1) {
        for (int rowsel = 0; rowsel < 16; ++rowsel) {
            // if (xSemaphoreTake(xSemaphore, (TickType_t) 0) == pdTRUE) {
                taskENTER_CRITICAL();
                fb.copy_foreground_row(rowsel, gc_row[0]);
                fb.copy_foreground_row(rowsel + 16, gc_row[1]);
                taskEXIT_CRITICAL();

            //     xSemaphoreGive(xSemaphore);
            // }

            for (int bit = 0; bit < 8; ++bit) {
                hub75_data_rgb888_set_shift(pio, sm_data, data_prog_offs, bit);
                for (int x = 0; x < FB_WIDTH; ++x) {
                    pio_sm_put_blocking(pio, sm_data, gc_row[0][x]);
                    pio_sm_put_blocking(pio, sm_data, gc_row[1][x]);
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
