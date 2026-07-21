#include "radar.h"

#include <math.h>
#include <stdint.h>

#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "ping/ping_sock.h"

static const char *TAG = "RADAR";

/* Lissage de la référence "signal au repos" -- plus petit = référence plus
 * lente à s'adapter (donc plus sensible aux variations soutenues). */
#define BASELINE_ALPHA 0.02f

/* Échelle empirique convertissant un écart moyen (en unités CSI brutes,
 * int8) en pourcentage 0-100. Première valeur (6.0) beaucoup trop
 * sensible en test réel -- même sans mouvement, le niveau tournait en
 * continu à 30-90%. Augmentée pour laisser une vraie marge entre le
 * bruit ambiant et un déplacement franc ; à raffiner avec des tests
 * "immobile" vs "main qui bouge devant la carte". */
#define DEVIATION_SCALE 25.0f

/* Détection de mouvement basée directement sur l'écart brut (unités CSI),
 * PAS sur le niveau affiché en % -- découplé de DEVIATION_SCALE pour ne
 * pas dépendre du calibrage visuel de la jauge. Mesuré sur matériel réel
 * 2026-07-21 : bruit au repos ~0.1-0.3, main devant la carte ~2-5. */
#define MOTION_DEVIATION_THRESHOLD 1.2f
/* Doit dépasser l'intervalle de rafraîchissement de l'UI (1s) pour être
 * fiablement visible au moins une fois -- avec une fenêtre plus courte,
 * un pic entre deux polls pouvait déjà être expiré au prochain tick. */
#define MOTION_HOLD_US (2000 * 1000)

/* Un seul paquet CSI bruité peut dépasser le seuil sans aucun mouvement
 * réel (bruit RF ambiant) -- exiger N échantillons consécutifs avant de
 * déclencher, pour filtrer ce bruit tout en captant un mouvement réel
 * (qui affecte plusieurs trames de suite). */
#define MOTION_CONSECUTIVE_REQUIRED 2

/* Lissage du niveau affiché (distinct du lissage de la baseline) --
 * évite que la jauge saute violemment à chaque trame individuelle. */
#define LEVEL_SMOOTH_ALPHA 0.3f

static float s_baseline = 0.0f;
static float s_signal_level_smooth = 0.0f;
static int s_signal_level = 0;
static int s_consecutive_high = 0;
static int64_t s_motion_until_us = 0;
static uint32_t s_sample_count = 0;
static bool s_baseline_initialized = false;

static void csi_rx_cb(void *ctx, wifi_csi_info_t *info)
{
    (void)ctx;

    if (info == NULL || info->buf == NULL || info->len == 0)
        return;

    size_t start = info->first_word_invalid ? 4 : 0;
    if (start >= info->len)
        return;

    int64_t sum = 0;
    size_t count = info->len - start;
    for (size_t i = start; i < info->len; i++)
    {
        int8_t v = info->buf[i];
        sum += (v < 0) ? -v : v;
    }
    float metric = (float)sum / (float)count;

    if (!s_baseline_initialized)
    {
        s_baseline = metric;
        s_baseline_initialized = true;
    }

    float deviation = fabsf(metric - s_baseline);
    s_baseline = s_baseline * (1.0f - BASELINE_ALPHA) + metric * BASELINE_ALPHA;

    int level = (int)((deviation / DEVIATION_SCALE) * 100.0f);
    if (level < 0) level = 0;
    if (level > 100) level = 100;

    s_signal_level_smooth = s_signal_level_smooth * (1.0f - LEVEL_SMOOTH_ALPHA) + (float)level * LEVEL_SMOOTH_ALPHA;
    s_signal_level = (int)s_signal_level_smooth;

    if (deviation > MOTION_DEVIATION_THRESHOLD)
    {
        s_consecutive_high++;
        if (s_consecutive_high >= MOTION_CONSECUTIVE_REQUIRED)
        {
            s_motion_until_us = esp_timer_get_time() + MOTION_HOLD_US;
        }
    }
    else
    {
        s_consecutive_high = 0;
    }

    s_sample_count++;
}

esp_err_t radar_init(void)
{
    wifi_csi_config_t csi_config = {
        .lltf_en = true,
        .htltf_en = true,
        .stbc_htltf2_en = true,
        .ltf_merge_en = true,
        .channel_filter_en = true,
        .manu_scale = false,
        .shift = 0,
        .dump_ack_en = false,
    };

    esp_err_t err = esp_wifi_set_csi_config(&csi_config);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_wifi_set_csi_config failed: %s", esp_err_to_name(err));
        return err;
    }

    err = esp_wifi_set_csi_rx_cb(csi_rx_cb, NULL);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_wifi_set_csi_rx_cb failed: %s", esp_err_to_name(err));
        return err;
    }

    err = esp_wifi_set_csi(true);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_wifi_set_csi(true) failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "Radar (Wi-Fi CSI) initialized");
    return ESP_OK;
}

static esp_ping_handle_t s_ping_hdl = NULL;

esp_err_t radar_start_probing(void)
{
    if (s_ping_hdl != NULL)
        return ESP_ERR_INVALID_STATE;

    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif == NULL)
        return ESP_FAIL;

    esp_netif_ip_info_t ip_info;
    if (esp_netif_get_ip_info(netif, &ip_info) != ESP_OK || ip_info.gw.addr == 0)
    {
        ESP_LOGW(TAG, "No gateway IP yet, cannot start probing");
        return ESP_FAIL;
    }

    esp_ping_config_t config = ESP_PING_DEFAULT_CONFIG();
    config.target_addr.type = IPADDR_TYPE_V4;
    config.target_addr.u_addr.ip4.addr = ip_info.gw.addr;
    config.count = ESP_PING_COUNT_INFINITE;
    config.interval_ms = 150;
    config.timeout_ms = 500;
    config.task_stack_size = 3072;
    config.task_prio = 3;

    esp_ping_callbacks_t cbs = {0};

    esp_err_t err = esp_ping_new_session(&config, &cbs, &s_ping_hdl);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_ping_new_session failed: %s", esp_err_to_name(err));
        return err;
    }

    err = esp_ping_start(s_ping_hdl);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_ping_start failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "Probing gateway for CSI density (150ms interval)");
    return ESP_OK;
}

int radar_get_signal_level(void)
{
    return s_signal_level;
}

bool radar_motion_detected(void)
{
    return esp_timer_get_time() < s_motion_until_us;
}

uint32_t radar_get_sample_count(void)
{
    return s_sample_count;
}
