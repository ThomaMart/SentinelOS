#include "protocol.h"

#include <string.h>

#include "driver/uart.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"
#include "psa/crypto.h"

#include "config.h"
#include "iff_secret.h"
#include "ota.h"
#include "system_info.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "PROTOCOL";

static const uint8_t PROTOCOL_MAGIC[4] = {0xAA, 0x55, 0xC3, 0x3C};

#define PROTOCOL_UART UART_NUM_0
#define PROTOCOL_RX_BUF_SIZE 256

typedef enum
{
    CMD_PING = 0x01,
    CMD_GET_INFO = 0x02,
    CMD_OTA_CHECK = 0x03,
    CMD_IFF_CHALLENGE = 0x04,
    CMD_IFF_RESPONSE = 0x05,
} protocol_cmd_t;

typedef enum
{
    RESP_OK = 0x80,
    RESP_PONG = 0x81,
    RESP_INFO = 0x82,
    RESP_IFF_CHALLENGE = 0x83,
    RESP_IFF_RESULT = 0x84,
    RESP_ERROR = 0xFF,
} protocol_resp_t;

/* IFF (Identification Friend or Foe) : challenge/response HMAC-SHA256.
 * L'hôte demande un nonce (CMD_IFF_CHALLENGE), doit prouver qu'il connaît
 * IFF_SECRET_KEY en renvoyant HMAC-SHA256(clé, nonce) (CMD_IFF_RESPONSE)
 * avant expiration -- un nonce à usage unique empêche le rejeu d'une
 * réponse capturée. */
#define IFF_NONCE_LEN 16
#define IFF_HMAC_LEN 32
#define IFF_NONCE_TIMEOUT_US (30 * 1000 * 1000)

static uint8_t s_iff_nonce[IFF_NONCE_LEN];
static bool s_iff_nonce_valid = false;
static int64_t s_iff_nonce_issued_us = 0;

static void put_u16_le(uint8_t *out, uint16_t v)
{
    out[0] = (uint8_t)(v & 0xFF);
    out[1] = (uint8_t)(v >> 8);
}

static void put_u32_le(uint8_t *out, uint32_t v)
{
    out[0] = (uint8_t)(v & 0xFF);
    out[1] = (uint8_t)((v >> 8) & 0xFF);
    out[2] = (uint8_t)((v >> 16) & 0xFF);
    out[3] = (uint8_t)((v >> 24) & 0xFF);
}

static void send_frame(uint8_t type, const uint8_t *payload, uint16_t payload_len)
{
    uint8_t header[4 + 2 + 1];
    memcpy(header, PROTOCOL_MAGIC, 4);
    put_u16_le(&header[4], payload_len);
    header[6] = type;

    uint8_t crc_input[1 + PROTOCOL_MAX_PAYLOAD];
    crc_input[0] = type;
    if (payload_len > 0)
    {
        memcpy(&crc_input[1], payload, payload_len);
    }
    uint16_t crc = protocol_crc16_ccitt(crc_input, (size_t)payload_len + 1);

    uint8_t crc_bytes[2];
    put_u16_le(crc_bytes, crc);

    uart_write_bytes(PROTOCOL_UART, (const char *)header, sizeof(header));
    if (payload_len > 0)
    {
        uart_write_bytes(PROTOCOL_UART, (const char *)payload, payload_len);
    }
    uart_write_bytes(PROTOCOL_UART, (const char *)crc_bytes, sizeof(crc_bytes));
}

static void send_error(uint8_t code)
{
    send_frame(RESP_ERROR, &code, 1);
}

static bool compute_hmac(const uint8_t *data, size_t data_len, uint8_t out_mac[IFF_HMAC_LEN])
{
    psa_key_attributes_t attr = PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_usage_flags(&attr, PSA_KEY_USAGE_SIGN_MESSAGE);
    psa_set_key_algorithm(&attr, PSA_ALG_HMAC(PSA_ALG_SHA_256));
    psa_set_key_type(&attr, PSA_KEY_TYPE_HMAC);

    mbedtls_svc_key_id_t key_id;
    if (psa_import_key(&attr, IFF_SECRET_KEY, sizeof(IFF_SECRET_KEY), &key_id) != PSA_SUCCESS)
    {
        return false;
    }

    size_t mac_len = 0;
    psa_status_t status = psa_mac_compute(key_id, PSA_ALG_HMAC(PSA_ALG_SHA_256),
                                          data, data_len, out_mac, IFF_HMAC_LEN, &mac_len);
    psa_destroy_key(key_id);

    return status == PSA_SUCCESS && mac_len == IFF_HMAC_LEN;
}

static void handle_frame(uint8_t type, const uint8_t *payload, uint16_t payload_len)
{
    switch (type)
    {
    case CMD_PING:
        ESP_LOGI(TAG, "CMD_PING");
        send_frame(RESP_PONG, NULL, 0);
        break;

    case CMD_GET_INFO:
    {
        ESP_LOGI(TAG, "CMD_GET_INFO");
        const char *version = system_get_version();
        size_t version_len = strlen(version);
        if (version_len > 32) version_len = 32;

        uint8_t resp[4 + 4 + 4 + 1 + 32];
        put_u32_le(&resp[0], system_get_uptime());
        put_u32_le(&resp[4], system_get_free_heap());
        put_u32_le(&resp[8], system_get_minimum_free_heap());
        resp[12] = (uint8_t)version_len;
        memcpy(&resp[13], version, version_len);

        send_frame(RESP_INFO, resp, (uint16_t)(13 + version_len));
        break;
    }

    case CMD_OTA_CHECK:
    {
        ESP_LOGI(TAG, "CMD_OTA_CHECK");
        esp_err_t err = ota_check_update(SENTINELOS_UPDATE_MANIFEST_URL);
        uint8_t status = (err == ESP_OK) ? 0 : 1;
        send_frame(RESP_OK, &status, 1);
        break;
    }

    case CMD_IFF_CHALLENGE:
    {
        ESP_LOGI(TAG, "CMD_IFF_CHALLENGE");
        esp_fill_random(s_iff_nonce, IFF_NONCE_LEN);
        s_iff_nonce_valid = true;
        s_iff_nonce_issued_us = esp_timer_get_time();
        send_frame(RESP_IFF_CHALLENGE, s_iff_nonce, IFF_NONCE_LEN);
        break;
    }

    case CMD_IFF_RESPONSE:
    {
        ESP_LOGI(TAG, "CMD_IFF_RESPONSE");
        uint8_t friend_status = 0;

        bool nonce_fresh = s_iff_nonce_valid &&
                          (esp_timer_get_time() - s_iff_nonce_issued_us) < IFF_NONCE_TIMEOUT_US;

        if (nonce_fresh && payload_len == IFF_HMAC_LEN)
        {
            uint8_t expected_mac[IFF_HMAC_LEN];
            if (compute_hmac(s_iff_nonce, IFF_NONCE_LEN, expected_mac) &&
                memcmp(expected_mac, payload, IFF_HMAC_LEN) == 0)
            {
                friend_status = 1;
            }
        }

        /* Nonce à usage unique : invalidé qu'il ait matché ou non, pour
         * empêcher un rejeu de la même réponse. */
        s_iff_nonce_valid = false;

        ESP_LOGI(TAG, "IFF result: %s", friend_status ? "FRIEND" : "UNKNOWN");
        send_frame(RESP_IFF_RESULT, &friend_status, 1);
        break;
    }

    default:
        ESP_LOGW(TAG, "Unknown command: 0x%02x", type);
        send_error(0x01);
        break;
    }
}

static void protocol_task(void *arg)
{
    (void)arg;

    static protocol_parser_t parser;
    protocol_parser_reset(&parser);

    uint8_t buf[64];

    /* Watchdog applicatif : voir le commentaire équivalent dans
     * display_task (display.c) -- même principe, cette tâche tourne en
     * continu et doit prouver qu'elle avance. */
    esp_task_wdt_add(NULL);

    while (1)
    {
        int len = uart_read_bytes(PROTOCOL_UART, buf, sizeof(buf), pdMS_TO_TICKS(50));
        for (int i = 0; i < len; i++)
        {
            protocol_frame_t frame;
            if (protocol_parser_feed(&parser, buf[i], &frame))
            {
                handle_frame(frame.type, frame.payload, frame.payload_len);
            }
        }

        esp_task_wdt_reset();
    }
}

esp_err_t protocol_init(void)
{
    esp_err_t err = uart_driver_install(PROTOCOL_UART, PROTOCOL_RX_BUF_SIZE, 0, 0, NULL, 0);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "uart_driver_install failed: %s", esp_err_to_name(err));
        return err;
    }

    if (xTaskCreate(protocol_task, "protocol", 4096, NULL, 4, NULL) != pdPASS)
    {
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Protocol initialized (UART0, shared with console)");
    return ESP_OK;
}
