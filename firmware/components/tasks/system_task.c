#include "system_task.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_heap_caps.h"
#include "esp_log.h"

static const char *TAG = "SYSTEM";

static void system_task(void *pvParameters)
{
    while (1)
    {
        ESP_LOGI(TAG, "Free Heap : %u bytes", esp_get_free_heap_size());

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void system_task_start(void)
{
    xTaskCreate(
        system_task,
        "system_task",
        4096,
        NULL,
        5,
        NULL);
}