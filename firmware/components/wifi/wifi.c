#include "wifi.h"
#include "config.h"
#include <string.h>

#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"

static const char *TAG = "wifi";

static bool s_connected = false;

static void wifi_event_handler(
    void *arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void *event_data)
{
    if (event_base == WIFI_EVENT)
    {
        switch (event_id)
        {
            case WIFI_EVENT_STA_START:
                ESP_LOGI(TAG, "Connecting...");
                esp_wifi_connect();
                break;

            case WIFI_EVENT_STA_DISCONNECTED:
                s_connected = false;
                ESP_LOGW(TAG, "Disconnected");
                esp_wifi_connect();
                break;

            default:
                break;
        }
    }

    if (event_base == IP_EVENT &&
        event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;

        s_connected = true;

        ESP_LOGI(
            TAG,
            "Connected - IP: " IPSTR,
            IP2STR(&event->ip_info.ip));
    }
}

esp_err_t wifi_init(void)
{
    ESP_LOGI(TAG, "Initializing Wi-Fi...");

    esp_err_t err;

    err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE)
        return err;

    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE)
        return err;

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    ESP_RETURN_ON_ERROR(
        esp_wifi_init(&cfg),
        TAG,
        "esp_wifi_init failed");

    ESP_RETURN_ON_ERROR(
        esp_event_handler_register(
            WIFI_EVENT,
            ESP_EVENT_ANY_ID,
            &wifi_event_handler,
            NULL),
        TAG,
        "event register failed");

    ESP_RETURN_ON_ERROR(
        esp_event_handler_register(
            IP_EVENT,
            IP_EVENT_STA_GOT_IP,
            &wifi_event_handler,
            NULL),
        TAG,
        "event register failed");

    wifi_config_t wifi_config = {0};

    strncpy(
        (char *)wifi_config.sta.ssid,
        config_get_wifi_ssid(),
        sizeof(wifi_config.sta.ssid));

    strncpy(
        (char *)wifi_config.sta.password,
        config_get_wifi_password(),
        sizeof(wifi_config.sta.password));

    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_RETURN_ON_ERROR(
        esp_wifi_set_mode(WIFI_MODE_STA),
        TAG,
        "set mode failed");

    ESP_RETURN_ON_ERROR(
        esp_wifi_set_config(
            WIFI_IF_STA,
            &wifi_config),
        TAG,
        "set config failed");

    ESP_RETURN_ON_ERROR(
        esp_wifi_start(),
        TAG,
        "start failed");

    return ESP_OK;
}

esp_err_t wifi_connect(void)
{
    return esp_wifi_connect();
}

void wifi_disconnect(void)
{
    esp_wifi_disconnect();
}

bool wifi_is_connected(void)
{
    return s_connected;
}