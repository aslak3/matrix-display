#if PICO_SDK
#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>

#include <hardware/pwm.h>
#elif ESP32_SDK
#warning "Buzzer is not yet supported on ESP32 boards; playing tones will be a noop"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#endif

#include "messages.h"
#include "rtttl.h"
#include "matrix_display.h"

extern QueueHandle_t buzzer_queue; // For listening

void play_note(RTTTLNote_t note);

uint slice_num = 0;
void buzzer_task(void *dummy)
{
    vTaskCoreAffinitySet(NULL, 1 << 0);
    DEBUG_printf("%s: core%u\n", pcTaskGetName(NULL), GET_CORE_NUMBER());

#if PICO_SDK
    gpio_set_function(BUZZER_PIN, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(BUZZER_PIN);
    // Set the base clock to 125MHz/250 = 500KHz
    pwm_set_clkdiv_int_frac(slice_num, 250, 0);
#endif

    while (1) {
        MessageBuzzer_t buzzer = {};
        RTTTL rtttl;

        if (xQueueReceive(buzzer_queue, &buzzer, portMAX_DELAY == pdTRUE)) {
            switch (buzzer.message_type) {
                case MESSAGE_BUZZER_SIMPLE:
                    DEBUG_printf("Buzzer simple\n");

                    switch (buzzer.simple_type) {
                        case BUZZER_SIMPLE_NOTIFICATION:
                            DEBUG_printf("Notification (non critical)\n");
                            for (int i = 0; i < 2; i++) {
                                play_note(RTTTLNote_t {
                                    frequency_hz: 1000, duration_ms: 100
                                });
                                vTaskDelay(200 / portTICK_PERIOD_MS);

                                play_note(RTTTLNote_t {
                                    frequency_hz: 500, duration_ms: 100
                                });
                                vTaskDelay(2000 / portTICK_PERIOD_MS);
                            }
                            break;

                        case BUZZER_SIMPLE_CRITICAL_NOTIFICATION:
                            DEBUG_printf("Notification (critical)\n");
                            for (int i = 0; i < 8; i++) {
                                play_note(RTTTLNote_t {
                                    frequency_hz: 500, duration_ms: 200
                                });
                                vTaskDelay(200 / portTICK_PERIOD_MS);

                                play_note(RTTTLNote_t {
                                    frequency_hz: 150, duration_ms: 200
                                });
                                vTaskDelay(1000 / portTICK_PERIOD_MS);
                            }

                            play_note(RTTTLNote_t {
                                frequency_hz: 100, duration_ms: 2000
                            });
                            break;

                        case BUZZER_SIMPLE_PORCH:
                            DEBUG_printf("Porch\n");
                            for (int j = 0; j < 5; j++) {
                                play_note(RTTTLNote_t {
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
                        RTTTLNote_t note;
                        while (rtttl.get_next_note(&note)) {
                            play_note(note);
                        }
                    }
                    else {
                        DEBUG_printf("Could not parse RTTTL data\n");
                        play_note(RTTTLNote_t {
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

void play_note(RTTTLNote_t note)
{
    DEBUG_printf("play_note: frequency_hz: %d, duration_ms: %d\n",
        note.frequency_hz, note.duration_ms);

#if PICO_SDK
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
#elif ESP32_SDK
    DEBUG_printf("Buzzer is not implemented on ESP32");
#endif
}
