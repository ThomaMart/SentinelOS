#include "diag.h"

#include "storage.h"

#include "esp_core_dump.h"
#include "esp_log.h"
#include "esp_system.h"

static const char *TAG = "DIAG";

#define NVS_KEY_BOOT_COUNT  "diag_boot_n"
#define NVS_KEY_CRASH_COUNT "diag_crash_n"

static const char *s_last_reason_str = "Unknown";
static bool s_last_was_crash = false;
static uint32_t s_boot_count = 0;
static uint32_t s_crash_count = 0;

static const char *reason_to_str(esp_reset_reason_t reason, bool *out_is_crash)
{
    switch (reason)
    {
    case ESP_RST_POWERON:
        *out_is_crash = false;
        return "Power-on";
    case ESP_RST_EXT:
        *out_is_crash = false;
        return "External pin";
    case ESP_RST_SW:
        *out_is_crash = false;
        return "Software reset";
    case ESP_RST_DEEPSLEEP:
        *out_is_crash = false;
        return "Deep sleep wake";
    case ESP_RST_SDIO:
        *out_is_crash = false;
        return "SDIO";
    case ESP_RST_PANIC:
        *out_is_crash = true;
        return "Panic";
    case ESP_RST_INT_WDT:
        *out_is_crash = true;
        return "Interrupt watchdog";
    case ESP_RST_TASK_WDT:
        *out_is_crash = true;
        return "Task watchdog";
    case ESP_RST_WDT:
        *out_is_crash = true;
        return "Other watchdog";
    case ESP_RST_BROWNOUT:
        *out_is_crash = true;
        return "Brownout";
    default:
        *out_is_crash = false;
        return "Unknown";
    }
}

esp_err_t diag_init(void)
{
    esp_reset_reason_t reason = esp_reset_reason();
    s_last_reason_str = reason_to_str(reason, &s_last_was_crash);

    if (storage_get_u32(NVS_KEY_BOOT_COUNT, &s_boot_count) != ESP_OK)
    {
        s_boot_count = 0;
    }
    if (storage_get_u32(NVS_KEY_CRASH_COUNT, &s_crash_count) != ESP_OK)
    {
        s_crash_count = 0;
    }

    s_boot_count++;
    storage_set_u32(NVS_KEY_BOOT_COUNT, s_boot_count);

    if (s_last_was_crash)
    {
        s_crash_count++;
        storage_set_u32(NVS_KEY_CRASH_COUNT, s_crash_count);
        ESP_LOGW(TAG, "Last reset was abnormal: %s (crash count: %lu)",
                s_last_reason_str, (unsigned long)s_crash_count);
    }
    else
    {
        ESP_LOGI(TAG, "Last reset: %s (boot count: %lu)",
                s_last_reason_str, (unsigned long)s_boot_count);
    }

    if (diag_has_coredump())
    {
        ESP_LOGW(TAG, "A core dump is present in flash (idf.py coredump-info to inspect)");
    }

    return ESP_OK;
}

bool diag_has_coredump(void)
{
    return esp_core_dump_image_check() == ESP_OK;
}

const char *diag_get_last_reset_reason_str(void)
{
    return s_last_reason_str;
}

bool diag_last_reset_was_crash(void)
{
    return s_last_was_crash;
}

uint32_t diag_get_crash_count(void)
{
    return s_crash_count;
}

uint32_t diag_get_boot_count(void)
{
    return s_boot_count;
}
