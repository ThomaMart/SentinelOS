#include <stdio.h>

#include "app.h"
#include "config.h"
#include "diag.h"
#include "i2c_bus.h"
#include "ota.h"
#include "protocol.h"
#include "radar.h"
#include "rwr.h"
#include "sdcard.h"
#include "storage.h"

#include "display.h"
#include "system_info.h"
#include "wifi.h"

#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define WIFI_CONNECT_TIMEOUT_MS 10000
#define WIFI_CONNECT_POLL_MS    500

static const char *TAG = "APP";

static void ui_ota_check_trigger(void)
{
    ESP_LOGI(TAG, "UI-triggered OTA check");
    esp_err_t err = ota_check_only(SENTINELOS_UPDATE_MANIFEST_URL);
    ESP_LOGI(TAG, "ota_check_only returned: %s", esp_err_to_name(err));
}

static void ui_ota_update_trigger(void)
{
    ESP_LOGI(TAG, "UI-triggered OTA update");
    esp_err_t err = ota_start_pending();
    ESP_LOGI(TAG, "ota_start_pending returned: %s", esp_err_to_name(err));
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    system_init();

    ESP_ERROR_CHECK(storage_init());
    ESP_ERROR_CHECK(diag_init());
    ESP_ERROR_CHECK(ota_init());
    ESP_ERROR_CHECK(protocol_init());

    esp_err_t i2c_err = i2c_bus_init();
    ESP_LOGI(TAG, "i2c_bus_init returned: %s", esp_err_to_name(i2c_err));

    display_init();
    display_set_ota_check_callback(ui_ota_check_trigger);
    display_set_ota_update_callback(ui_ota_update_trigger);

    esp_err_t sd_err = sdcard_init();
    ESP_LOGI(TAG, "sdcard_init returned: %s", esp_err_to_name(sd_err));
    display_set_sdcard_status(sdcard_is_mounted(), sdcard_get_capacity_mb(), sdcard_get_free_mb());
    if (sdcard_is_mounted())
    {
        char boot_line[64];
        snprintf(boot_line, sizeof(boot_line), "Boot #%lu, reset: %s",
                (unsigned long)diag_get_boot_count(), diag_get_last_reset_reason_str());
        sdcard_append_log(boot_line);
    }

    wifi_init();

    esp_err_t radar_err = radar_init();
    ESP_LOGI(TAG, "radar_init returned: %s", esp_err_to_name(radar_err));

    esp_err_t rwr_err = rwr_init();
    ESP_LOGI(TAG, "rwr_init returned: %s", esp_err_to_name(rwr_err));

    ESP_LOGI(TAG, "Waiting for Wi-Fi connection...");

    int waited_ms = 0;
    while (!wifi_is_connected() && waited_ms < WIFI_CONNECT_TIMEOUT_MS)
    {
        vTaskDelay(pdMS_TO_TICKS(WIFI_CONNECT_POLL_MS));
        waited_ms += WIFI_CONNECT_POLL_MS;
    }

    ESP_LOGI(TAG, "Wait done: connected=%d waited_ms=%d", wifi_is_connected(), waited_ms);

    if (wifi_is_connected())
    {
        ota_confirm_valid();

        esp_err_t probe_err = radar_start_probing();
        ESP_LOGI(TAG, "radar_start_probing returned: %s", esp_err_to_name(probe_err));

        ESP_LOGI(TAG, "Calling ota_check_update...");
        esp_err_t err = ota_check_update(SENTINELOS_UPDATE_MANIFEST_URL);
        ESP_LOGI(TAG, "ota_check_update returned: %s", esp_err_to_name(err));
    }
}
