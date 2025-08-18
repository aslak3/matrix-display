#include <stdio.h>

#include <pico/stdlib.h>
#include <pico/cyw43_arch.h>

#include <hardware/pwm.h>

#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>

#include "matrix_display.h"

#include "messages.h"
#include "rtttl.h"

#define BUZZER_PIN 27

extern QueueHandle_t buzzer_queue; // For listening

void play_note(uint slice_num, rtttl_note_t note);

void buzzer_task(void *dummy)
{
#if FREE_RTOS_KERNEL_SMP
    vTaskCoreAffinitySet(NULL, 1 << 0);
    DEBUG_printf("%s: core%u\n", pcTaskGetName(NULL), get_core_num());
#endif

    gpio_set_function(BUZZER_PIN, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(BUZZER_PIN);
    // Set the base clock to 125MHz/250 = 500KHz
    pwm_set_clkdiv_int_frac(slice_num, 250, 0);

    while (1) {
        message_buzzer_t buzzer = {};
        rtttl rtttl;

        if (xQueueReceive(buzzer_queue, &buzzer, 1000 / portTICK_PERIOD_MS) == pdTRUE) {
            switch (buzzer.message_type) {
                case MESSAGE_BUZZER_SIMPLE:
                    DEBUG_printf("Buzzer simple\n");

                    switch (buzzer.simple_type) {
                        case BUZZER_SIMPLE_NOTIFICATION:
                            DEBUG_printf("Notification (non critical)\n");
                            for (int i = 0; i < 2; i++) {
                                play_note(slice_num, rtttl_note_t {
                                    frequency_hz: 1000, duration_ms: 100
                                });
                                vTaskDelay(200 / portTICK_PERIOD_MS);

                                play_note(slice_num, rtttl_note_t {
                                    frequency_hz: 500, duration_ms: 100
                                });
                                vTaskDelay(2000 / portTICK_PERIOD_MS);
                            }
                            break;

                        case BUZZER_SIMPLE_CRITICAL_NOTIFICATION:
                            DEBUG_printf("Notification (critical)\n");
                            for (int i = 0; i < 8; i++) {
                                play_note(slice_num, rtttl_note_t {
                                    frequency_hz: 500, duration_ms: 200
                                });
                                vTaskDelay(200 / portTICK_PERIOD_MS);

                                play_note(slice_num, rtttl_note_t {
                                    frequency_hz: 150, duration_ms: 200
                                });
                                vTaskDelay(1000 / portTICK_PERIOD_MS);
                            }

                            play_note(slice_num, rtttl_note_t {
                                frequency_hz: 100, duration_ms: 2000
                            });
                            break;

                        case BUZZER_SIMPLE_PORCH:
                            DEBUG_printf("Porch\n");
                            for (int j = 0; j < 5; j++) {
                                play_note(slice_num, rtttl_note_t {
                                    frequency_hz: 250, duration_ms: 50
                                });
                                vTaskDelay(50 / portTICK_PERIOD_MS);
                            }
                            break;

                        default:
                            DEBUG_printf("Bad buzzer simple type %d\n", buzzer.simple_type);
                            break;
                    }
                    break;

                case MESSAGE_BUZZER_RTTTL:
                    DEBUG_printf("Buzzer RTTTL, tune data: %s\n", buzzer.rtttl_tune);
                    if (rtttl.parse_header(buzzer.rtttl_tune)) {
                        rtttl_note_t note;
                        while (rtttl.get_next_note(&note)) {
                            play_note(slice_num, note);
                        }
                    }
                    else {
                        DEBUG_printf("Could not parse RTTTL data\n");
                        play_note(slice_num, rtttl_note_t {
                            frequency_hz: 100, duration_ms: 2000
                        });
                    }
                    break;

                default:
                    DEBUG_printf("Bad buzzer message type %d\n", buzzer.message_type);
                    break;
            }
        }
    }
}

void play_note(uint slice_num, rtttl_note_t note)
{
    DEBUG_printf("play_note: frequency_hz: %d, duration_ms: %d\n",
        note.frequency_hz, note.duration_ms);

    if (note.frequency_hz) {
        int wrap = 500*1000 / note.frequency_hz;
        pwm_set_wrap(slice_num, wrap);
        pwm_set_chan_level(slice_num, PWM_CHAN_B, wrap / 2); // 50% duty
        
        pwm_set_counter(slice_num, 0);
        pwm_set_enabled(slice_num, true);
    }

    vTaskDelay(note.duration_ms / portTICK_PERIOD_MS);

    if (note.frequency_hz) {
        pwm_set_enabled(slice_num, false);
    }
}
