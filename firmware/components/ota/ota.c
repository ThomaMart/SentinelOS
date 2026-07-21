#include "ota.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "cJSON.h"
#include "config.h"
#include "display.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "ota_pubkey.h"

#include "mbedtls/pk.h"
#include "psa/crypto.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "OTA";

typedef enum
{
    OTA_STATE_IDLE = 0,
    OTA_STATE_CONNECTING,
    OTA_STATE_DOWNLOADING,
    OTA_STATE_VERIFYING,
    OTA_STATE_FINISHED,
    OTA_STATE_FAILED
} ota_state_t;

#define OTA_MAX_SIGNATURE_LEN 128

typedef struct
{
    bool running;
    int progress;
    ota_state_t state;
    esp_err_t last_error;
    char url[256];
    char expected_sha256[65];
    uint8_t expected_signature[OTA_MAX_SIGNATURE_LEN];
    size_t expected_signature_len;

    /* Résultat du dernier ota_check_only() qui a trouvé une version différente,
     * en attente d'un ota_start_pending() explicite. */
    bool pending_available;
    char pending_url[256];
    char pending_sha256[65];
    char pending_signature[256];
} ota_context_t;

static ota_context_t s_ota;
static TaskHandle_t s_ota_task = NULL;

static void ota_task(void *arg);

esp_err_t ota_init(void)
{
    memset(&s_ota, 0, sizeof(s_ota));
    s_ota.state = OTA_STATE_IDLE;
    s_ota.last_error = ESP_OK;

    if (psa_crypto_init() != PSA_SUCCESS)
    {
        ESP_LOGE(TAG, "psa_crypto_init failed");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "OTA initialized");
    return ESP_OK;
}

static int hex_nibble(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static size_t hex_decode(const char *hex, uint8_t *out, size_t out_max)
{
    size_t hex_len = strlen(hex);
    if (hex_len == 0 || (hex_len % 2) != 0) return 0;

    size_t out_len = hex_len / 2;
    if (out_len > out_max) return 0;

    for (size_t i = 0; i < out_len; i++)
    {
        int hi = hex_nibble(hex[i * 2]);
        int lo = hex_nibble(hex[i * 2 + 1]);
        if (hi < 0 || lo < 0) return 0;
        out[i] = (uint8_t)((hi << 4) | lo);
    }

    return out_len;
}

esp_err_t ota_start(const char *url, const char *expected_sha256, const char *expected_signature_hex)
{
    if (!url) return ESP_ERR_INVALID_ARG;
    if (s_ota.running) return ESP_ERR_INVALID_STATE;

    strlcpy(s_ota.url, url, sizeof(s_ota.url));

    s_ota.expected_sha256[0] = '\0';
    if (expected_sha256)
    {
        strlcpy(s_ota.expected_sha256, expected_sha256, sizeof(s_ota.expected_sha256));
    }

    s_ota.expected_signature_len = 0;
    if (expected_signature_hex)
    {
        s_ota.expected_signature_len = hex_decode(expected_signature_hex,
                                                   s_ota.expected_signature,
                                                   sizeof(s_ota.expected_signature));
        if (s_ota.expected_signature_len == 0)
        {
            ESP_LOGW(TAG, "Manifest signature is not valid hex, ignoring it");
        }
    }

    if (xTaskCreate(ota_task, "ota", 8192, NULL, 5, &s_ota_task) != pdPASS)
        return ESP_FAIL;

    return ESP_OK;
}

bool ota_is_running(void)
{
    return s_ota.running;
}

int ota_get_progress(void)
{
    return s_ota.progress;
}

void ota_confirm_valid(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t state;

    if (esp_ota_get_state_partition(running, &state) != ESP_OK)
        return;

    if (state == ESP_OTA_IMG_PENDING_VERIFY)
    {
        esp_ota_mark_app_valid_cancel_rollback();
        ESP_LOGI(TAG, "Firmware marked valid, rollback cancelled");
    }
}

static esp_err_t ota_check_update_internal(const char *manifest_url, bool auto_start)
{
    if (!manifest_url) return ESP_ERR_INVALID_ARG;

    display_set_status("Connecting to update server...");

    esp_http_client_config_t http_cfg = {
        .url = manifest_url,
        .timeout_ms = 5000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&http_cfg);
    if (!client)
    {
        display_set_status("Update check failed");
        return ESP_FAIL;
    }

    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Manifest fetch failed: %s", esp_err_to_name(err));
        display_set_status("Update check failed");
        esp_http_client_cleanup(client);
        return err;
    }

    esp_http_client_fetch_headers(client);

    char body[512] = {0};
    int len = esp_http_client_read_response(client, body, sizeof(body) - 1);

    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    if (len <= 0)
    {
        ESP_LOGE(TAG, "Manifest empty response");
        display_set_status("Update check failed");
        return ESP_FAIL;
    }

    cJSON *json = cJSON_ParseWithLength(body, len);
    if (!json)
    {
        ESP_LOGE(TAG, "Manifest parse failed");
        display_set_status("Update check failed");
        return ESP_ERR_INVALID_RESPONSE;
    }

    cJSON *version = cJSON_GetObjectItem(json, "version");
    cJSON *firmware = cJSON_GetObjectItem(json, "firmware");
    cJSON *sha256 = cJSON_GetObjectItem(json, "sha256");
    cJSON *signature = cJSON_GetObjectItem(json, "signature");

    esp_err_t ret = ESP_OK;

    if (!cJSON_IsString(version) || !cJSON_IsString(firmware))
    {
        ESP_LOGE(TAG, "Manifest missing 'version' or 'firmware'");
        display_set_status("Update check failed");
        ret = ESP_ERR_INVALID_RESPONSE;
    }
    else if (strcmp(version->valuestring, SENTINELOS_VERSION) == 0)
    {
        ESP_LOGI(TAG, "Firmware up to date (%s)", SENTINELOS_VERSION);
        display_set_status("No update available");
        s_ota.pending_available = false;
        display_set_ota_available(false);
    }
    else
    {
        if (!cJSON_IsString(sha256))
        {
            ESP_LOGW(TAG, "Manifest has no 'sha256', OTA will run without integrity check");
        }
        if (!cJSON_IsString(signature))
        {
            ESP_LOGW(TAG, "Manifest has no 'signature', OTA will run without authenticity check");
        }

        ESP_LOGI(TAG, "Update available: %s -> %s", SENTINELOS_VERSION, version->valuestring);

        char status[48];
        snprintf(status, sizeof(status), "Update available: %s", version->valuestring);
        display_set_status(status);

        if (auto_start)
        {
            ret = ota_start(firmware->valuestring,
                             cJSON_IsString(sha256) ? sha256->valuestring : NULL,
                             cJSON_IsString(signature) ? signature->valuestring : NULL);
        }
        else
        {
            strlcpy(s_ota.pending_url, firmware->valuestring, sizeof(s_ota.pending_url));
            s_ota.pending_sha256[0] = '\0';
            if (cJSON_IsString(sha256))
            {
                strlcpy(s_ota.pending_sha256, sha256->valuestring, sizeof(s_ota.pending_sha256));
            }
            s_ota.pending_signature[0] = '\0';
            if (cJSON_IsString(signature))
            {
                strlcpy(s_ota.pending_signature, signature->valuestring, sizeof(s_ota.pending_signature));
            }
            s_ota.pending_available = true;
            display_set_ota_available(true);
        }
    }

    cJSON_Delete(json);
    return ret;
}

esp_err_t ota_check_update(const char *manifest_url)
{
    return ota_check_update_internal(manifest_url, true);
}

esp_err_t ota_check_only(const char *manifest_url)
{
    return ota_check_update_internal(manifest_url, false);
}

esp_err_t ota_start_pending(void)
{
    if (!s_ota.pending_available) return ESP_ERR_INVALID_STATE;

    s_ota.pending_available = false;
    display_set_ota_available(false);

    return ota_start(s_ota.pending_url,
                      s_ota.pending_sha256[0] != '\0' ? s_ota.pending_sha256 : NULL,
                      s_ota.pending_signature[0] != '\0' ? s_ota.pending_signature : NULL);
}

static bool ota_get_update_digest(uint8_t digest[32])
{
    const esp_partition_t *part = esp_ota_get_next_update_partition(NULL);
    if (!part)
    {
        ESP_LOGE(TAG, "No update partition found for verification");
        return false;
    }

    if (esp_partition_get_sha256(part, digest) != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to compute partition SHA256");
        return false;
    }

    return true;
}

static bool ota_verify_sha256(const uint8_t digest[32])
{
    char hex[65];
    for (int i = 0; i < 32; i++)
    {
        snprintf(&hex[i * 2], 3, "%02x", digest[i]);
    }

    if (strcasecmp(hex, s_ota.expected_sha256) != 0)
    {
        ESP_LOGE(TAG, "SHA256 mismatch: expected %s, got %s", s_ota.expected_sha256, hex);
        return false;
    }

    ESP_LOGI(TAG, "SHA256 verified: %s", hex);
    return true;
}

/**
 * Vérifie la signature ECDSA (DER) du digest de mise à jour avec la clé
 * publique embarquée. Le schéma signe SHA256(digest) où digest est déjà le
 * hash de l'image (celui vérifié par ota_verify_sha256), donc côté outillage
 * de release la signature s'obtient avec un simple "sign" ECDSA/SHA-256 sur
 * ces 32 octets.
 */
static bool ota_verify_signature(const uint8_t digest[32])
{
    uint8_t digest_hash[32];
    size_t digest_hash_len = 0;

    if (psa_hash_compute(PSA_ALG_SHA_256, digest, 32,
                          digest_hash, sizeof(digest_hash), &digest_hash_len) != PSA_SUCCESS)
    {
        ESP_LOGE(TAG, "Failed to hash digest for signature verification");
        return false;
    }

    mbedtls_pk_context pk;
    mbedtls_pk_init(&pk);

    const uint8_t *pubkey = OTA_SIGNING_PUBKEY_DER;
    int ret = mbedtls_pk_parse_public_key(&pk, pubkey, OTA_SIGNING_PUBKEY_DER_LEN);
    if (ret != 0)
    {
        ESP_LOGE(TAG, "Failed to parse embedded public key: -0x%04x", (unsigned int)-ret);
        mbedtls_pk_free(&pk);
        return false;
    }

    ret = mbedtls_pk_verify(&pk, MBEDTLS_MD_SHA256,
                             digest_hash, sizeof(digest_hash),
                             s_ota.expected_signature, s_ota.expected_signature_len);

    mbedtls_pk_free(&pk);

    if (ret != 0)
    {
        ESP_LOGE(TAG, "Signature verification failed: -0x%04x", (unsigned int)-ret);
        return false;
    }

    ESP_LOGI(TAG, "Signature verified");
    return true;
}

static void ota_task(void *arg)
{
    (void)arg;

    esp_https_ota_handle_t handle = NULL;
    esp_err_t err;

    s_ota.running = true;
    s_ota.progress = 0;
    s_ota.state = OTA_STATE_CONNECTING;
    s_ota.last_error = ESP_OK;

    esp_http_client_config_t http_cfg = {
        .url = s_ota.url,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    esp_https_ota_config_t ota_cfg = {
        .http_config = &http_cfg,
    };

    ESP_LOGI(TAG, "Starting OTA: %s", s_ota.url);
    display_set_ota_progress(0);
    display_set_status("Downloading firmware...");

    err = esp_https_ota_begin(&ota_cfg, &handle);
    if (err != ESP_OK)
        goto cleanup;

    s_ota.state = OTA_STATE_DOWNLOADING;

    int image_size = esp_https_ota_get_image_size(handle);
    int last_logged = -1;

    while ((err = esp_https_ota_perform(handle)) == ESP_ERR_HTTPS_OTA_IN_PROGRESS)
    {
        if (image_size > 0)
        {
            s_ota.progress = (esp_https_ota_get_image_len_read(handle) * 100) / image_size;
            display_set_ota_progress(s_ota.progress);

            if (s_ota.progress / 10 != last_logged)
            {
                last_logged = s_ota.progress / 10;
                ESP_LOGI(TAG, "Progress: %d%%", s_ota.progress);

                char status[32];
                snprintf(status, sizeof(status), "Downloading... %d%%", s_ota.progress);
                display_set_status(status);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }

    if (err != ESP_OK)
        goto cleanup;

    s_ota.state = OTA_STATE_VERIFYING;
    display_set_status("Verifying update...");

    if (s_ota.expected_sha256[0] != '\0' || s_ota.expected_signature_len > 0)
    {
        uint8_t digest[32];
        if (!ota_get_update_digest(digest))
        {
            display_set_status("Integrity check failed");
            err = ESP_ERR_INVALID_CRC;
            goto cleanup;
        }

        if (s_ota.expected_sha256[0] != '\0' && !ota_verify_sha256(digest))
        {
            display_set_status("Integrity check failed");
            err = ESP_ERR_INVALID_CRC;
            goto cleanup;
        }

        if (s_ota.expected_signature_len > 0 && !ota_verify_signature(digest))
        {
            display_set_status("Signature check failed");
            err = ESP_ERR_INVALID_CRC;
            goto cleanup;
        }
    }

    err = esp_https_ota_finish(handle);
    handle = NULL;

    if (err != ESP_OK)
        goto cleanup;

    s_ota.progress = 100;
    s_ota.state = OTA_STATE_FINISHED;
    s_ota.running = false;

    ESP_LOGI(TAG, "OTA successful, rebooting...");
    display_set_ota_progress(100);
    display_set_status("Update complete, rebooting...");
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();

cleanup:
    if (handle)
        esp_https_ota_abort(handle);

    s_ota.running = false;

    if (err != ESP_OK)
    {
        s_ota.state = OTA_STATE_FAILED;
        s_ota.last_error = err;
        ESP_LOGE(TAG, "OTA failed: %s", esp_err_to_name(err));
        display_set_status("Update failed");
    }

    vTaskDelete(NULL);
}
