#include "rogue_ap.h"

#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "esp_wifi.h"

static const char *TAG = "ROGUE_AP";

typedef struct
{
    char ssid[33];
    char bssid_str[18];
    int8_t rssi;
    bool flagged;
    char flag_reason[24];
} rogue_ap_entry_t;

static rogue_ap_entry_t s_entries[ROGUE_AP_MAX_ENTRIES];
static uint8_t s_count = 0;
static uint8_t s_flagged_count = 0;

static void mark_flagged(uint8_t idx, const char *reason)
{
    if (!s_entries[idx].flagged)
    {
        s_flagged_count++;
    }
    s_entries[idx].flagged = true;
    snprintf(s_entries[idx].flag_reason, sizeof(s_entries[idx].flag_reason), "%s", reason);
}

esp_err_t rogue_ap_scan(void)
{
    wifi_scan_config_t scan_config = {
        .show_hidden = false,
    };

    esp_err_t err = esp_wifi_scan_start(&scan_config, true);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_wifi_scan_start failed: %s", esp_err_to_name(err));
        return err;
    }

    uint16_t ap_num = ROGUE_AP_MAX_ENTRIES;
    wifi_ap_record_t records[ROGUE_AP_MAX_ENTRIES];
    err = esp_wifi_scan_get_ap_records(&ap_num, records);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_wifi_scan_get_ap_records failed: %s", esp_err_to_name(err));
        return err;
    }

    s_count = (uint8_t)ap_num;
    s_flagged_count = 0;

    for (uint8_t i = 0; i < s_count; i++)
    {
        snprintf(s_entries[i].ssid, sizeof(s_entries[i].ssid), "%s", (const char *)records[i].ssid);
        snprintf(s_entries[i].bssid_str, sizeof(s_entries[i].bssid_str),
                "%02X:%02X:%02X:%02X:%02X:%02X",
                records[i].bssid[0], records[i].bssid[1], records[i].bssid[2],
                records[i].bssid[3], records[i].bssid[4], records[i].bssid[5]);
        s_entries[i].rssi = records[i].rssi;
        s_entries[i].flagged = false;
        s_entries[i].flag_reason[0] = '\0';

        if (records[i].authmode == WIFI_AUTH_OPEN || records[i].authmode == WIFI_AUTH_WEP)
        {
            mark_flagged(i, "Weak security");
        }
    }

    /* Détection "evil twin" : même SSID (non vide) porté par au moins
     * deux BSSID différents -- marque toutes les entrées concernées. */
    for (uint8_t i = 0; i < s_count; i++)
    {
        if (s_entries[i].ssid[0] == '\0')
            continue;

        for (uint8_t j = i + 1; j < s_count; j++)
        {
            if (strcmp(s_entries[i].ssid, s_entries[j].ssid) == 0 &&
                strcmp(s_entries[i].bssid_str, s_entries[j].bssid_str) != 0)
            {
                mark_flagged(i, "Duplicate SSID");
                mark_flagged(j, "Duplicate SSID");
            }
        }
    }

    ESP_LOGI(TAG, "Scan done: %u network(s), %u flagged", (unsigned)s_count, (unsigned)s_flagged_count);
    return ESP_OK;
}

uint8_t rogue_ap_get_count(void)
{
    return s_count;
}

uint8_t rogue_ap_get_flagged_count(void)
{
    return s_flagged_count;
}

const char *rogue_ap_get_ssid(uint8_t idx)
{
    if (idx >= s_count)
        return "";
    return s_entries[idx].ssid;
}

const char *rogue_ap_get_bssid_str(uint8_t idx)
{
    if (idx >= s_count)
        return "";
    return s_entries[idx].bssid_str;
}

int8_t rogue_ap_get_rssi(uint8_t idx)
{
    if (idx >= s_count)
        return 0;
    return s_entries[idx].rssi;
}

bool rogue_ap_is_flagged(uint8_t idx)
{
    if (idx >= s_count)
        return false;
    return s_entries[idx].flagged;
}

const char *rogue_ap_get_flag_reason(uint8_t idx)
{
    if (idx >= s_count)
        return "";
    return s_entries[idx].flag_reason;
}
