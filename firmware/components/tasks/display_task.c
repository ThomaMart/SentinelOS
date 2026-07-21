#include "display_task.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"

static const char *TAG = "DISPLAY";

static void display_task(void *pvParameters)
{
    while (1)
    {
        ESP_LOGI(TAG, "Display task alive");

        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}

void display_task_start(void)
{
    xTaskCreate(
        display_task,
        "display_task",
        4096,
        NULL,
        3,
        NULL);
}