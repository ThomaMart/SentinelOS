#include "sdcard.h"

#include <stdio.h>

#include "driver/sdspi_host.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

#include "hardware.h"

static const char *TAG = "SDCARD";

#define SD_CS_GPIO GPIO_NUM_5

static sdmmc_card_t *s_card = NULL;
static bool s_mounted = false;

esp_err_t sdcard_init(void)
{
    esp_vfs_fat_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 4,
        .allocation_unit_size = 16 * 1024,
    };

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = LCD_SPI_HOST;
    host.max_freq_khz = SDMMC_FREQ_PROBING;

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = SD_CS_GPIO;
    slot_config.host_id = LCD_SPI_HOST;

    esp_err_t err = esp_vfs_fat_sdspi_mount(SDCARD_MOUNT_POINT, &host, &slot_config,
                                            &mount_config, &s_card);
    if (err != ESP_OK)
    {
        if (err == ESP_FAIL)
        {
            ESP_LOGW(TAG, "Failed to mount FATFS (no card, or unreadable filesystem)");
        }
        else
        {
            ESP_LOGW(TAG, "SD card init failed: %s", esp_err_to_name(err));
        }
        s_mounted = false;
        return err;
    }

    s_mounted = true;
    ESP_LOGI(TAG, "SD card mounted at %s (%s)", SDCARD_MOUNT_POINT, s_card->cid.name);
    return ESP_OK;
}

bool sdcard_is_mounted(void)
{
    return s_mounted;
}

uint32_t sdcard_get_capacity_mb(void)
{
    if (!s_mounted)
        return 0;

    uint64_t total_bytes = 0, free_bytes = 0;
    if (esp_vfs_fat_info(SDCARD_MOUNT_POINT, &total_bytes, &free_bytes) != ESP_OK)
        return 0;

    return (uint32_t)(total_bytes / (1024 * 1024));
}

uint32_t sdcard_get_free_mb(void)
{
    if (!s_mounted)
        return 0;

    uint64_t total_bytes = 0, free_bytes = 0;
    if (esp_vfs_fat_info(SDCARD_MOUNT_POINT, &total_bytes, &free_bytes) != ESP_OK)
        return 0;

    return (uint32_t)(free_bytes / (1024 * 1024));
}

void sdcard_append_log(const char *text)
{
    if (!s_mounted || text == NULL)
        return;

    FILE *f = fopen(SDCARD_MOUNT_POINT "/sentinelos.log", "a");
    if (f == NULL)
    {
        ESP_LOGW(TAG, "Could not open log file for append");
        return;
    }

    fprintf(f, "[%lld] %s\n", (long long)esp_timer_get_time() / 1000, text);
    fclose(f);
}
