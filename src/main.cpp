#include <stdio.h>
#include <math.h>
#include <inttypes.h>
#include <string.h>

#if PICO_SDK

#include "pico/stdlib.h"
#include <hardware/gpio.h>
#include <hardware/spi.h>
#include <hardware/dma.h>
#include <hardware/watchdog.h>
#include <pico/cyw43_arch.h>

#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <semphr.h>

#include "hub75.pio.h"

#elif ESP32_SDK

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

#include "nvs_flash.h"
#include "driver/gpio.h"
#include "driver/dedic_gpio.h"
#include "rom/ets_sys.h"

#endif

#include "matrix_display.h"

#include "animation.h"

#if PICO_SDK
#if SPI_TO_FPGA
#define FPGA_RESET_PIN 26
#else
#define DATA_BASE_PIN 2
#define DATA_N_PINS 6
#define ROWSEL_BASE_PIN 8
#define ROWSEL_N_PINS 4

#define CLK_PIN 13
#define STROBE_PIN 14
#define OEN_PIN 15
#endif
#elif ESP32_SDK
#define RED1_PIN GPIO_NUM_42
#define GREEN1_PIN GPIO_NUM_41
#define BLUE1_PIN GPIO_NUM_40
#define RED2_PIN GPIO_NUM_38
#define GREEN2_PIN GPIO_NUM_39
#define BLUE2_PIN GPIO_NUM_37

#define ROWSEL_A_PIN GPIO_NUM_45
#define ROWSEL_B_PIN GPIO_NUM_36
#define ROWSEL_C_PIN GPIO_NUM_48
#define ROWSEL_D_PIN GPIO_NUM_35
#define ROWSEL_E_PIN GPIO_NUM_21

#define CLK_PIN GPIO_NUM_2
#define STROBE_PIN GPIO_NUM_47
#define OEN_PIN GPIO_NUM_14
#endif

void animate_task(void *dummy);
void matrix_task(void *dummy);
void i2c_task(void *dummy);
void buzzer_task(void *dummy);

void vApplicationTickHook(void);

extern void mqtt_task(void *dummy);

QueueHandle_t animate_queue;
QueueHandle_t matrix_queue;
QueueHandle_t mqtt_queue;
QueueHandle_t i2c_queue;
QueueHandle_t buzzer_queue;

#if PICO_SDK
int main(void)
{
    stdio_init_all();
#elif ESP32_SDK
extern "C" void app_main(void)
{
#endif
#if PICO_SDK
    // Let USB UART wake up on a listener, schedular not running yet so can't use vTaskDelay
    sleep_ms(1000);
#elif ESP32_SDK
    vTaskDelay(1000 / portTICK_PERIOD_MS);
#endif

    DEBUG_printf("Hello, matrix here\n");

    srand(0);

#if ESP32_SDK
    ESP_ERROR_CHECK(nvs_flash_init());
#endif

    animate_queue = xQueueCreate(3, sizeof(message_anim_t));
    mqtt_queue = xQueueCreate(3, sizeof(message_mqtt_t));
    i2c_queue = xQueueCreate(3, sizeof(message_i2c_t));
    buzzer_queue = xQueueCreate(3, sizeof(message_buzzer_t));

    matrix_queue = xQueueCreate(1, sizeof(fb_t));

    xTaskCreate(&animate_task, "Animate Task", 4096, NULL, 0, NULL);
    xTaskCreate(&mqtt_task, "MQTT Task", 4096, NULL, 0, NULL);
    xTaskCreate(&i2c_task, "I2C Task", 4096, NULL, 0, NULL);
    xTaskCreate(&buzzer_task, "Buzzer Task", 4096, NULL, 0, NULL);

    xTaskCreate(&matrix_task, "Matrix Task", 1024, NULL, 10, NULL);

#if PICO_SDK
    vTaskStartScheduler();
#endif

    while (1) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        DEBUG_printf("Hello there... bottom of app_main()\n");
    }

#if PICO_SDK
    return 0;
#endif
}

framebuffer fb;
animation anim(fb);
message_anim_t message;

void animate_task(void *dummy)
{
    vTaskCoreAffinitySet(NULL, 1 << 0);
    DEBUG_printf("%s: core%u\n", pcTaskGetName(NULL), GET_CORE_NUMBER());

#if PICO_SDK
    if (watchdog_enable_caused_reboot()) {
        DEBUG_printf("Restart caused by watchdog!\n");
    } else {
        DEBUG_printf("Clean start; not caused by watchdog\n");
    }

    bool watchdog_enabled = false;
    int last_network_message_seconds = 0;
    int seconds_counter = 0;
#endif
    while (1)
    {
        while (xQueueReceive(animate_queue, &message, 0) == pdTRUE) {
            DEBUG_printf("New message, type: %d\n", message.message_type);

#if PICO_SDK
            if (message.message_type != MESSAGE_ANIM_DS3231) {
                last_network_message_seconds = seconds_counter;
            }
#endif

            switch (message.message_type) {
                case MESSAGE_ANIM_WEATHER:
                    anim.new_weather_data(&message.weather_data);
                    break;

                case MESSAGE_ANIM_MEDIA_PLAYER:
                    anim.new_media_player_data(&message.media_player_data);
                    break;

                case MESSAGE_ANIM_CALENDAR:
                    anim.new_calendar_data(&message.calendar_data);
                    break;

                case MESSAGE_ANIM_SCROLLER:
                    anim.new_scroller_data(&message.scroller_data);
                    break;

                case MESSAGE_ANIM_TRANSPORT:
                    anim.new_transport_data(&message.transport_data);
                    break;

                case MESSAGE_ANIM_NOTIFICATION:
                    anim.new_notification(&message.notification);
                    break;

                case MESSAGE_ANIM_PORCH:
                    anim.new_porch(&message.porch);
                    break;

                case MESSAGE_ANIM_BRIGHTNESS:
                    fb.set_brightness(message.brightness);
                    break;

                case MESSAGE_ANIM_GRAYSCALE:
                    fb.set_grayscale(message.grayscale);
                    break;

                case MESSAGE_ANIM_CONFIGURATION:
                    anim.update_configuration(&message.configuration);
                    break;

                case MESSAGE_ANIM_DS3231:
#if PICO_SDK
                    seconds_counter++;
                    if (! watchdog_enabled) {
                        // On the first message from the RTC task, Enable the watchdog
                        // requiring the watchdog to be updated every 2s or the MCU will
                        // reboot.
                        watchdog_enable(2000, 1);
                        watchdog_enabled = true;
                        DEBUG_printf("Watchdog enabled\n");
                    }
                    else {
                        // Need a network related message every 10 minutes.
                        if (last_network_message_seconds + 10 * 60 > seconds_counter) {
                            // We must now get a new time every two seconds or we will reboot
                            watchdog_update();
                        }
                    }
#endif
                    anim.new_ds3231(&message.ds3231);
                    break;

                default:
                    DEBUG_printf("Unknown message type: %d\n", message.message_type);
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
    vTaskCoreAffinitySet(NULL, 1 << 1);
    DEBUG_printf("%s: core%u\n", pcTaskGetName(NULL), GET_CORE_NUMBER());

#if PICO_SDK
#if SPI_TO_FPGA
    gpio_init(FPGA_RESET_PIN);
    gpio_set_dir(FPGA_RESET_PIN, true);
    gpio_put(FPGA_RESET_PIN, false);

    static fb_t output_fb;

    spi_init(spi_default, 10 * 1000 * 1000);
    gpio_set_function(PICO_DEFAULT_SPI_RX_PIN, GPIO_FUNC_SPI);
    gpio_set_function(PICO_DEFAULT_SPI_SCK_PIN, GPIO_FUNC_SPI);
    gpio_set_function(PICO_DEFAULT_SPI_TX_PIN, GPIO_FUNC_SPI);
    gpio_init(PICO_DEFAULT_SPI_CSN_PIN);
    gpio_set_dir(PICO_DEFAULT_SPI_CSN_PIN, true);

    gpio_put(FPGA_RESET_PIN, true);

    const uint dma_tx = dma_claim_unused_channel(true);

    DEBUG_printf("Configure TX DMA\n");
    dma_channel_config dma_config_c = dma_channel_get_default_config(dma_tx);
    // TODO: investigate DMA_SIZE_32 transfers
    channel_config_set_transfer_data_size(&dma_config_c, DMA_SIZE_8);
    channel_config_set_dreq(&dma_config_c, spi_get_dreq(spi_default, true));

    dma_channel_configure(dma_tx, &dma_config_c,
        &spi_get_hw(spi_default)->dr,               // write address
        NULL,                                       // read address (set later)
        FB_WIDTH * FB_HEIGHT * sizeof(uint32_t),    // element count
        false);                                     // don't start it now

    sleep_ms(1000);

    while (1) {
        fb.atomic_fore_copy_out(&output_fb);

        gpio_put(PICO_DEFAULT_SPI_CSN_PIN, false);

        // Set the read address to the top of frame and trigger
        dma_channel_set_read_addr(dma_tx, &output_fb.uint32, true);
        dma_channel_wait_for_finish_blocking(dma_tx);

        fb.atomic_fore_copy_out(&output_fb);

        gpio_put(PICO_DEFAULT_SPI_CSN_PIN, true);

        // Set the read address to the top of frame and trigger
        dma_channel_set_read_addr(dma_tx, &output_fb.uint32, true);
        dma_channel_wait_for_finish_blocking(dma_tx);
    }
#else
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
#endif
#elif ESP32_SDK
    gpio_config_t io_conf = {
        .pin_bit_mask =
            (1ULL << CLK_PIN) | (1ULL << STROBE_PIN) | (1ULL << OEN_PIN) |
            (1ULL << ROWSEL_A_PIN) | (1ULL << ROWSEL_B_PIN) |
            (1ULL << ROWSEL_C_PIN) | (1ULL << ROWSEL_D_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    ESP_ERROR_CHECK(gpio_config(&io_conf));

    // Define data GPIO pins for the bundle
    int data_pins[] = {
        RED1_PIN, GREEN1_PIN, BLUE1_PIN, RED2_PIN, GREEN2_PIN, BLUE2_PIN,
    };
    dedic_gpio_bundle_handle_t data_bundle;

    dedic_gpio_bundle_config_t data_bundle_config = {
        .gpio_array = data_pins,
        .array_size = sizeof(data_pins) / sizeof(int),
        .flags = {
            .out_en = 1,
        }
    };

    // Create the rowsel GPIO bundle
    dedic_gpio_new_bundle(&data_bundle_config, &data_bundle);

    DEBUG_printf("Created dedic GPIO\n");

    static fb_t output_fb;

    while (1) {
        // fb.atomic_fore_copy_out(&output_fb);
        // for (int pwm_threshold = 16; pwm_threshold < 256; pwm_threshold *= 2) {
            for (int y = 0; y < 16; y++) {
                gpio_set_level(ROWSEL_A_PIN, y & 0x1 ? true : false);
                gpio_set_level(ROWSEL_B_PIN, y & 0x2 ? true : false);
                gpio_set_level(ROWSEL_C_PIN, y & 0x4 ? true : false);
                gpio_set_level(ROWSEL_D_PIN, y & 0x8 ? true : false);

                for (int x = 63; x >= 0; x--) {
                    // uint32_t colour = 0;
                    // if (output_fb.rgb[x][y].red > pwm_threshold) {
                    //     colour |= 0x01;
                    // }
                    // if (output_fb.rgb[x][y + 16].red < pwm_threshold) {
                    //     colour |= 0x02;
                    // }
                    // if (output_fb.rgb[x][y].green < pwm_threshold) {
                    //     colour |= 0x04;
                    // }
                    // if (output_fb.rgb[x][y + 16].green < pwm_threshold) {
                    //     colour |= 0x08;
                    // }
                    // if (output_fb.rgb[x][y].blue < pwm_threshold) {
                    //     colour |= 0x10;
                    // }
                    // if (output_fb.rgb[x][y + 16].blue < pwm_threshold) {
                    //     colour |= 0x02;
                    // }

                    dedic_gpio_bundle_write(data_bundle, 0x3f, 0x3f);//colour);

                    // clock high
                    gpio_set_level(CLK_PIN, true);
                    // ets_delay_us(1);
                    // clock low
                    gpio_set_level(CLK_PIN, false);
                    // ets_delay_us(1);
                }

                // oe high
                gpio_set_level(OEN_PIN, true);
                // latch high
                gpio_set_level(STROBE_PIN, true);
                // latch low
                gpio_set_level(STROBE_PIN, false);
                // oe low
                gpio_set_level(OEN_PIN, false);

                // ets_delay_us(100);
        // }
            }
    }
#endif
}

#if ESP32_SDK
void panic(const char *format, ...)
{
    va_list args;
    va_start(args, format);

    // Add a prefix for this panic
    printf("PANIC!!! : ");
    vprintf(format, args);
    printf("\n");

    va_end(args);

    while(1);
}
#endif
