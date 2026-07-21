#ifndef PROTOCOL_H
#define PROTOCOL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

/*
 * Trame :
 *   MAGIC (4o) | LEN (2o, LE, taille du PAYLOAD) | TYPE (1o) | PAYLOAD (LEN o) | CRC16 (2o, LE)
 *
 * CRC16 = CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF) calculé sur TYPE + PAYLOAD.
 *
 * La partie parsing (protocol_parser_*) est pure et sans effet de bord --
 * testée isolément dans test_apps/protocol. Le dispatch des commandes
 * (OTA, system info...) reste dans protocol.c et n'est pas exposé ici.
 */

#define PROTOCOL_MAX_PAYLOAD 128

typedef struct
{
    uint8_t type;
    uint8_t payload[PROTOCOL_MAX_PAYLOAD];
    uint16_t payload_len;
} protocol_frame_t;

typedef enum
{
    PROTOCOL_PARSE_STATE_MAGIC = 0,
    PROTOCOL_PARSE_STATE_LEN_LO,
    PROTOCOL_PARSE_STATE_LEN_HI,
    PROTOCOL_PARSE_STATE_TYPE,
    PROTOCOL_PARSE_STATE_PAYLOAD,
    PROTOCOL_PARSE_STATE_CRC_LO,
    PROTOCOL_PARSE_STATE_CRC_HI,
} protocol_parse_state_t;

typedef struct
{
    protocol_parse_state_t state;
    size_t magic_index;
    uint16_t payload_len;
    uint8_t type;
    uint8_t payload[PROTOCOL_MAX_PAYLOAD];
    size_t payload_index;
    uint16_t crc_received;
} protocol_parser_t;

/**
 * @brief CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF).
 */
uint16_t protocol_crc16_ccitt(const uint8_t *data, size_t len);

/**
 * @brief Réinitialise le parseur (attend un nouveau magic depuis le début).
 */
void protocol_parser_reset(protocol_parser_t *parser);

/**
 * @brief Fait avancer le parseur d'un octet.
 *
 * @return true si une trame complète et valide (CRC correct) vient d'être
 *         décodée dans *out_frame. false sinon -- trame incomplète, ou
 *         trame invalide (magic cassé, longueur hors bornes, CRC invalide)
 *         silencieusement rejetée ; le parseur s'est alors réinitialisé et
 *         continue de scanner le flux sans bloquer.
 */
bool protocol_parser_feed(protocol_parser_t *parser, uint8_t byte, protocol_frame_t *out_frame);

/**
 * @brief Démarre le protocole de commande binaire framé sur UART0.
 *
 * Partage le port série déjà utilisé pour les logs (esp_console). Voir
 * tools/uart_proto_client.py pour un client hôte de test.
 *
 * Lance une tâche FreeRTOS dédiée à la lecture/dispatch.
 */
esp_err_t protocol_init(void);

#ifdef __cplusplus
}
#endif

#endif /* PROTOCOL_H */
