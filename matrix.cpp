#include <stdio.h>

#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "pico/cyw43_arch.h"

#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>

#include "framebuffer.hpp"

void animate_task(void *dummy);
void matrix_task(void *dummy);

void vApplicationTickHook(void);

extern void mqtt_task(void *dummy);

/* WIRING
 *
 * R1   GP8
 * B1   GP9
 * R2   GP10
 * B2   GP11
 * A    GP12
 * C    GP13
 * CLK  GP14
 * OE   GP15
 *
 * G1   GP20
 * G2   GP19
 * E    GP18
 * B    GP17
 * D    GP21
 * LAT  GP16
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
    xTaskCreate(&matrix_task, "Matrix Task", 256, NULL, 0, NULL);
    xTaskCreate(&mqtt_task, "MQTT Task", 1024, NULL, 0, NULL);

    vTaskStartScheduler();

    while(1);

    return 0;
}

framebuffer fb;

char topic_message[256] = "TESTING";

inline void framebuffer::swap(void)
{
    rgb_t (*temp)[FB_WIDTH][FB_HEIGHT];

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

        if (xSemaphoreTake(xSemaphore, (TickType_t) 10) == pdTRUE) {
            fb.swap();
            xSemaphoreGive(xSemaphore);
        }

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

    for (int g = 8; g <= 21; g++) gpio_init(g);

    uint32_t clock = 1 << 14;
    uint32_t latch = 1 << 16;
    uint32_t oe =    1 << 15;
    uint32_t r1 =    1 << 8;
    uint32_t g1 =    1 << 20;
    uint32_t b1 =    1 << 9;
    uint32_t r2 =    1 << 10;
    uint32_t g2 =    1 << 19;
    uint32_t b2 =    1 << 11;
    uint32_t a =     1 << 12;
    uint32_t b =     1 << 17;
    uint32_t c =     1 << 13;
    uint32_t d =     1 << 21;
    uint32_t e =     1 << 18;

    uint32_t mask = r1 | g1 | b1 | r2 | g2 | b2 | a | b | c | d | e | latch | clock | oe;

    gpio_set_dir_out_masked(mask);

    int y;

    while (1) {
        for (int same = 0; same < 8; same++) {
            for (y = 0; y < 16; y++) {

                uint32_t row = 0;
                if (y & 0x1) row |= a;
                if (y & 0x2) row |= b;
                if (y & 0x4) row |= c;
                if (y & 0x8) row |= d;

                if (xSemaphoreTake(xSemaphore, (TickType_t) 0) == pdTRUE) {
                    for (int x = 63; x >= 0; x--) {
                        uint32_t colour = 0;
                        if ((*fb.foreground_rgb)[x][y].red > (same * 32)) {
                            colour |= r1;
                        }
                        if ((*fb.foreground_rgb)[x][y + 16].red > (same * 32)) {
                            colour |= r2;
                        }

                        if ((*fb.foreground_rgb)[x][y].green  > (same * 32)) {
                            colour |= g1;
                        }
                        if ((*fb.foreground_rgb)[x][y + 16].green > (same * 32)) {
                            colour |= g2;
                        }

                        if ((*fb.foreground_rgb)[x][y].blue > (same * 32)) {
                            colour |= b1;
                        }
                        if ((*fb.foreground_rgb)[x][y + 16].blue > (same * 32)) {
                            colour |= b2;
                        }

                        // clock high
                        gpio_put_masked(mask, row | oe | colour | clock);
                        // clock low
                        gpio_put_masked(mask, row | oe | colour);
                    }

                    xSemaphoreGive(xSemaphore);
                }

                gpio_put_masked(mask, row | oe);

                // latch high
                gpio_put_masked(mask, row | oe | latch);
                // latch low
                gpio_put_masked(mask, row | oe);

                gpio_put_masked(mask, row);

                sleep_us(50);
            }
        }
    }
}
