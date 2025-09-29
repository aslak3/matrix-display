#include <stdio.h>
#include <string.h>
#include <time.h>

#if PICO_SDK
#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>

#include <pico/stdlib.h>
#include <pico/cyw43_arch.h>
#elif ESP32_SDK
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_event.h"
#include "esp_sntp.h"
#endif

#include "messages.h"
#include "matrix_display.h"

extern QueueHandle_t time_queue; // For listening
extern QueueHandle_t animate_queue;
extern QueueHandle_t mqtt_queue;

void time_task(void *dummy)
{
    vTaskCoreAffinitySet(NULL, 1 << 0);
    DEBUG_printf("%s: core%u\n", pcTaskGetName(NULL), GET_CORE_NUMBER());

    int old_tm_sec = -1;
    bool time_sync = false;
    static MessageAnim_t message_anim;
    message_anim.message_type = MESSAGE_ANIM_TIMEINFO;

    while (1) {
        struct tm timeinfo;

        time_t now = time(NULL);
        localtime_r(&now, &timeinfo);

        // See if we are at the top of the next second
        if ((timeinfo.tm_sec != old_tm_sec) && time_sync) {
            message_anim.timeinfo = timeinfo;
            if (xQueueSend(animate_queue, &message_anim, 10) != pdTRUE) {
                DEBUG_printf("Could not send clock data; dropping\n");
            }
        }

        old_tm_sec = timeinfo.tm_sec;

        MessageTime_t message_time;

        if (xQueueReceive(time_queue, &message_time, 0) == pdTRUE) {
            switch (message_time.message_type)
            {
                case MESSAGE_TIME_TIMESYNC:
                    DEBUG_printf("Clock set from NTP\n");
                    time_sync = true;        
                    break;
                
                default:
                    panic("Uknown message type in message to Time task\n");
                    break;
            }
        }

        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}
