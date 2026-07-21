#include "logger_task.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"

static const char *TAG = "LOGGER";

static void logger_task(void *pvParameters)
{
    while (1)
    {
        ESP_LOGI(TAG, "Logger alive");

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void logger_task_start(void)
{
    xTaskCreate(
        logger_task,
        "logger_task",
        4096,
        NULL,
        4,
        NULL);
}