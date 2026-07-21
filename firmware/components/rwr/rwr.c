#include "rwr.h"

#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "esp_wifi.h"

static const char *TAG = "RWR";

/* Nombre de trames deauth/disassoc observées dans une fenêtre glissante
 * au-delà duquel on considère qu'il s'agit d'une attaque active (rafale)
 * plutôt que du bruit ambiant normal (une déconnexion légitime isolée
 * génère au plus 1-2 trames). Outils type aireplay-ng envoient des
 * dizaines de trames par seconde en rafale. */
#define ATTACK_WINDOW_US        (1000 * 1000)
#define ATTACK_COUNT_THRESHOLD  5
/* Maintenu affiché quelques secondes après la dernière rafale pour
 * rester visible malgré le rafraîchissement UI à 1 Hz. */
#define ATTACK_HOLD_US          (5000 * 1000)

/* En-tête 802.11 management (non-QoS) : FC(2) Duration(2) Addr1(6)
 * Addr2(6, source) Addr3(6) SeqCtl(2) = 24 octets. */
#define DOT11_ADDR2_OFFSET 10
#define DOT11_HEADER_LEN   24

#define DOT11_SUBTYPE_DISASSOC 0x0A
#define DOT11_SUBTYPE_DEAUTH   0x0C

static uint32_t s_deauth_count = 0;
static uint32_t s_total_frames = 0;
static int64_t s_window_start_us = 0;
static int s_window_count = 0;
static int64_t s_attack_until_us = 0;
static char s_last_attacker[18] = "";

static void rwr_promiscuous_cb(void *buf, wifi_promiscuous_pkt_type_t type)
{
    if (type != WIFI_PKT_MGMT || buf == NULL)
        return;

    const wifi_promiscuous_pkt_t *pkt = (const wifi_promiscuous_pkt_t *)buf;
    if (pkt->rx_ctrl.sig_len < DOT11_HEADER_LEN)
        return;

    const uint8_t *payload = pkt->payload;
    uint8_t subtype = (payload[0] >> 4) & 0x0F;

    s_total_frames++;

    if (subtype != DOT11_SUBTYPE_DEAUTH && subtype != DOT11_SUBTYPE_DISASSOC)
        return;

    s_deauth_count++;

    const uint8_t *src = payload + DOT11_ADDR2_OFFSET;
    snprintf(s_last_attacker, sizeof(s_last_attacker), "%02X:%02X:%02X:%02X:%02X:%02X",
             src[0], src[1], src[2], src[3], src[4], src[5]);

    int64_t now = esp_timer_get_time();
    if (now - s_window_start_us > ATTACK_WINDOW_US)
    {
        s_window_start_us = now;
        s_window_count = 0;
    }
    s_window_count++;

    if (s_window_count >= ATTACK_COUNT_THRESHOLD)
    {
        s_attack_until_us = now + ATTACK_HOLD_US;
    }
}

esp_err_t rwr_init(void)
{
    wifi_promiscuous_filter_t filter = {
        .filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT,
    };

    esp_err_t err = esp_wifi_set_promiscuous_filter(&filter);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_wifi_set_promiscuous_filter failed: %s", esp_err_to_name(err));
        return err;
    }

    err = esp_wifi_set_promiscuous_rx_cb(rwr_promiscuous_cb);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_wifi_set_promiscuous_rx_cb failed: %s", esp_err_to_name(err));
        return err;
    }

    err = esp_wifi_set_promiscuous(true);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_wifi_set_promiscuous(true) failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "RWR (deauth/disassoc detection) initialized");
    return ESP_OK;
}

bool rwr_attack_detected(void)
{
    return esp_timer_get_time() < s_attack_until_us;
}

uint32_t rwr_get_deauth_count(void)
{
    return s_deauth_count;
}

uint32_t rwr_get_total_frames(void)
{
    return s_total_frames;
}

const char *rwr_get_last_attacker(void)
{
    return s_last_attacker;
}
