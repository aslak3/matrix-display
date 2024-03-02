#include <stdio.h>
#include <math.h>

#include <pico/stdlib.h>
#include <hardware/gpio.h>
#include <hardware/spi.h>
#include <hardware/dma.h>
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

#define FPGA_RESET_PIN 26
#define BUZZER_PIN 27

void animate_task(void *dummy);
void matrix_task(void *dummy);
void rtc_task(void *dummy);

void vApplicationTickHook(void);

extern void mqtt_task(void *dummy);

QueueHandle_t animate_queue;
QueueHandle_t rtc_queue;
QueueHandle_t matrix_queue;

int main(void)
{
    stdio_init_all();

    printf("Hello, matrix here\n");

    gpio_init(BUZZER_PIN);
    gpio_set_dir(BUZZER_PIN, true);
    gpio_put(BUZZER_PIN, false);

    gpio_init(FPGA_RESET_PIN);
    gpio_set_dir(FPGA_RESET_PIN, true);
    gpio_put(FPGA_RESET_PIN, false);

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
            printf("New message, type: %d\n", message.message_type);

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
                    if (message.brightness.type == BRIGHTNESS_RED) {
                        fb.set_brightness_red(message.brightness.intensity);
                    } else if (message.brightness.type == BRIGHTNESS_GREEN) {
                        fb.set_brightness_green(message.brightness.intensity);
                    } else if (message.brightness.type == BRIGHTNESS_BLUE) {
                        fb.set_brightness_blue(message.brightness.intensity);
                    } else {
                        printf("Invalid message.brightness.type\n");
                    }
                    break;

                case MESSAGE_GRAYSCALE:
                    fb.set_grayscale(message.grayscale);
                    break;

                case MESSAGE_CONFIGURATION:
                    anim.update_configuration(&message.configuration);
                    break;

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

    static fb_t output_fb;

    spi_init(spi_default, 10 * 1000 * 1000);
    gpio_set_function(PICO_DEFAULT_SPI_RX_PIN, GPIO_FUNC_SPI);
    gpio_set_function(PICO_DEFAULT_SPI_SCK_PIN, GPIO_FUNC_SPI);
    gpio_set_function(PICO_DEFAULT_SPI_TX_PIN, GPIO_FUNC_SPI);
    gpio_init(PICO_DEFAULT_SPI_CSN_PIN);
    gpio_set_dir(PICO_DEFAULT_SPI_CSN_PIN, true);

    gpio_put(FPGA_RESET_PIN, true);

    const uint dma_tx = dma_claim_unused_channel(true);

    printf("Configure TX DMA\n");
    dma_channel_config dma_config_c = dma_channel_get_default_config(dma_tx);
    // TODO: investigate DMA_SIZE_32 transfers
    channel_config_set_transfer_data_size(&dma_config_c, DMA_SIZE_8);
    channel_config_set_dreq(&dma_config_c, spi_get_dreq(spi_default, true));

    dma_channel_configure(dma_tx, &dma_config_c,
        &spi_get_hw(spi_default)->dr,               // write address
        NULL,                                       // read address (set later)
        FB_WIDTH * FB_HEIGHT * sizeof(uint32_t),    // element count
        false);                                     // don't start it now

    while (1) {
        fb.atomic_fore_copy_out(&output_fb);

        gpio_put(PICO_DEFAULT_SPI_CSN_PIN, false);

        // Set the read address to the top of frame and trigger
        dma_channel_set_read_addr(dma_tx, &output_fb.uint32, true);
        dma_channel_wait_for_finish_blocking(dma_tx);

        gpio_put(PICO_DEFAULT_SPI_CSN_PIN, true);
    }
}
