#include <stdio.h>

#include <pico/stdlib.h>
#include <pico/cyw43_arch.h>

#include <hardware/pwm.h>

#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>

#include "messages.h"

#define BUZZER_PIN 27

extern QueueHandle_t buzzer_queue; // For listening

void buzzer_task(void *dummy)
{
#if FREE_RTOS_KERNEL_SMP
    vTaskCoreAffinitySet(NULL, 1 << 0);
    printf("%s: core%u\n", pcTaskGetName(NULL), get_core_num());
#endif

    gpio_set_function(BUZZER_PIN, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(BUZZER_PIN);

    pwm_set_wrap(slice_num, 1000); // 1Khz
    pwm_set_chan_level(slice_num, PWM_CHAN_B, 500); // 50% duty



    while (1) {
        message_buzzer_t buzzer = {};

        if (xQueueReceive(buzzer_queue, &buzzer, 1000 / portTICK_PERIOD_MS) == pdTRUE) {
            printf("Buzz buzz buzz\n");

            switch (buzzer.message_type) {
                case MESSAGE_BUZZER_PLAY:
                    switch (buzzer.play_type) {
                        case BUZZER_PLAY_NOTIFICATION:
                            for (int i = 0; i < 2; i++) {
                                pwm_set_clkdiv(slice_num, 125.0); // Overall 1KHz
                                pwm_set_enabled(slice_num, true);
                                vTaskDelay(100 / portTICK_PERIOD_MS);
                                pwm_set_enabled(slice_num, false);   

                                vTaskDelay(200 / portTICK_PERIOD_MS);

                                pwm_set_clkdiv(slice_num, 250.0); // Overal 500Hz
                                pwm_set_enabled(slice_num, true);
                                vTaskDelay(100 / portTICK_PERIOD_MS);
                                pwm_set_enabled(slice_num, false);   

                                vTaskDelay(2000 / portTICK_PERIOD_MS);
                            }
                            break;

                        case BUZZER_PLAY_PORCH:
                            for (int j = 0; j < 5; j++) {
                                pwm_set_clkdiv(slice_num, 500.0); // Overall 250Hz
                                pwm_set_enabled(slice_num, true);
                                vTaskDelay(50 / portTICK_PERIOD_MS);
                                pwm_set_enabled(slice_num, false);   

                                vTaskDelay(50 / portTICK_PERIOD_MS);
                            }
                            break;
                        
                        default:
                            printf("Bad buzzer play type %d\n", buzzer.play_type);
                            break;
                    }
                    break;

                default:
                    printf("Bad buzzer message type %d\n", buzzer.message_type);
                    break;
            }
        }
    }
}