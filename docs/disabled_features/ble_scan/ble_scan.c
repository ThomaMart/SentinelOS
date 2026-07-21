#include "ble_scan.h"

#include <stdio.h>
#include <string.h>

#include "esp_log.h"

#include "host/ble_hs.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"

#include "ble_allowlist.h"

static const char *TAG = "BLE_SCAN";

#define BLE_SCAN_DURATION_MS 5000

typedef struct
{
    char name[32];
    char addr_str[18];
    uint8_t addr[6];
    int8_t rssi;
    bool is_friend;
} ble_scan_entry_t;

static ble_scan_entry_t s_entries[BLE_SCAN_MAX_DEVICES];
static uint8_t s_count = 0;
static volatile bool s_scanning = false;
static volatile bool s_host_synced = false;
static bool s_initialized = false;

static bool addr_is_friend(const uint8_t addr[6])
{
    for (size_t i = 0; i < BLE_ALLOWLIST_COUNT; i++)
    {
        if (memcmp(addr, BLE_ALLOWLIST[i], 6) == 0)
            return true;
    }
    return false;
}

static int find_entry(const uint8_t addr[6])
{
    for (uint8_t i = 0; i < s_count; i++)
    {
        if (memcmp(s_entries[i].addr, addr, 6) == 0)
            return i;
    }
    return -1;
}

static int gap_event_cb(struct ble_gap_event *event, void *arg)
{
    (void)arg;

    switch (event->type)
    {
    case BLE_GAP_EVENT_DISC:
    {
        const struct ble_gap_disc_desc *disc = &event->disc;

        int idx = find_entry(disc->addr.val);
        if (idx < 0)
        {
            if (s_count >= BLE_SCAN_MAX_DEVICES)
                break;
            idx = s_count++;
            memcpy(s_entries[idx].addr, disc->addr.val, 6);
            snprintf(s_entries[idx].addr_str, sizeof(s_entries[idx].addr_str),
                    "%02X:%02X:%02X:%02X:%02X:%02X",
                    disc->addr.val[5], disc->addr.val[4], disc->addr.val[3],
                    disc->addr.val[2], disc->addr.val[1], disc->addr.val[0]);
            s_entries[idx].name[0] = '\0';
            s_entries[idx].is_friend = addr_is_friend(disc->addr.val);
        }

        s_entries[idx].rssi = disc->rssi;

        struct ble_hs_adv_fields fields;
        if (ble_hs_adv_parse_fields(&fields, disc->data, disc->length_data) == 0 &&
            fields.name != NULL && fields.name_len > 0)
        {
            size_t n = fields.name_len < sizeof(s_entries[idx].name) - 1
                          ? fields.name_len
                          : sizeof(s_entries[idx].name) - 1;
            memcpy(s_entries[idx].name, fields.name, n);
            s_entries[idx].name[n] = '\0';
        }
        break;
    }

    case BLE_GAP_EVENT_DISC_COMPLETE:
        ESP_LOGI(TAG, "Scan complete: %u device(s)", (unsigned)s_count);
        s_scanning = false;
        break;

    default:
        break;
    }

    return 0;
}

esp_err_t ble_scan_start(void)
{
    if (s_scanning)
        return ESP_ERR_INVALID_STATE;

    uint8_t own_addr_type;
    int rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "ble_hs_id_infer_auto failed: %d", rc);
        return ESP_FAIL;
    }

    struct ble_gap_disc_params params = {0};
    params.passive = 1;
    params.filter_duplicates = 0;

    s_count = 0;
    s_scanning = true;

    rc = ble_gap_disc(own_addr_type, BLE_SCAN_DURATION_MS, &params, gap_event_cb, NULL);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "ble_gap_disc failed: %d", rc);
        s_scanning = false;
        return ESP_FAIL;
    }

    return ESP_OK;
}

bool ble_scan_is_running(void)
{
    return s_scanning;
}

uint8_t ble_scan_get_count(void)
{
    return s_count;
}

const char *ble_scan_get_name(uint8_t idx)
{
    if (idx >= s_count)
        return "";
    return s_entries[idx].name;
}

const char *ble_scan_get_addr_str(uint8_t idx)
{
    if (idx >= s_count)
        return "";
    return s_entries[idx].addr_str;
}

int8_t ble_scan_get_rssi(uint8_t idx)
{
    if (idx >= s_count)
        return 0;
    return s_entries[idx].rssi;
}

bool ble_scan_is_friend(uint8_t idx)
{
    if (idx >= s_count)
        return false;
    return s_entries[idx].is_friend;
}

static void ble_scan_on_reset(int reason)
{
    ESP_LOGW(TAG, "BLE host reset, reason=%d", reason);
}

static void ble_scan_on_sync(void)
{
    ESP_LOGI(TAG, "BLE host synced, ready to scan");
    s_host_synced = true;
}

bool ble_scan_is_ready(void)
{
    return s_host_synced;
}

static void nimble_host_task(void *param)
{
    (void)param;
    nimble_port_run();
    nimble_port_freertos_deinit();
}

esp_err_t ble_scan_init(void)
{
    if (s_initialized)
        return ESP_OK;

    esp_err_t err = nimble_port_init();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "nimble_port_init failed: %s", esp_err_to_name(err));
        return err;
    }

    ble_hs_cfg.reset_cb = ble_scan_on_reset;
    ble_hs_cfg.sync_cb = ble_scan_on_sync;

    nimble_port_freertos_init(nimble_host_task);

    s_initialized = true;
    ESP_LOGI(TAG, "BLE scan (NimBLE observer) initialized");
    return ESP_OK;
}
