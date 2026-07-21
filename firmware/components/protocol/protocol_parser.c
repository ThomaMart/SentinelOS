#include "protocol.h"

#include <string.h>

/*
 * MAGIC choisi improbable dans du texte de log ASCII ; toute donnée qui ne
 * correspond pas à une trame valide (magic, longueur ou CRC incohérents)
 * est simplement ignorée octet par octet -- le flux de logs esp_console
 * partage le même UART0 sans coordination particulière.
 *
 * Ce fichier n'a aucune dépendance vers OTA/Wi-Fi/system -- pur, testable
 * isolément (voir test_apps/protocol).
 */
static const uint8_t PROTOCOL_MAGIC[4] = {0xAA, 0x55, 0xC3, 0x3C};

uint16_t protocol_crc16_ccitt(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;

    for (size_t i = 0; i < len; i++)
    {
        crc ^= (uint16_t)data[i] << 8;
        for (int b = 0; b < 8; b++)
        {
            crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
        }
    }

    return crc;
}

void protocol_parser_reset(protocol_parser_t *parser)
{
    parser->state = PROTOCOL_PARSE_STATE_MAGIC;
    parser->magic_index = 0;
    parser->payload_len = 0;
    parser->payload_index = 0;
    parser->crc_received = 0;
}

bool protocol_parser_feed(protocol_parser_t *parser, uint8_t byte, protocol_frame_t *out_frame)
{
    switch (parser->state)
    {
    case PROTOCOL_PARSE_STATE_MAGIC:
        if (byte == PROTOCOL_MAGIC[parser->magic_index])
        {
            parser->magic_index++;
            if (parser->magic_index == sizeof(PROTOCOL_MAGIC))
            {
                parser->state = PROTOCOL_PARSE_STATE_LEN_LO;
            }
        }
        else
        {
            /* Resynchronise sans bloquer sur un octet de log qui matcherait partiellement. */
            parser->magic_index = (byte == PROTOCOL_MAGIC[0]) ? 1 : 0;
        }
        return false;

    case PROTOCOL_PARSE_STATE_LEN_LO:
        parser->payload_len = byte;
        parser->state = PROTOCOL_PARSE_STATE_LEN_HI;
        return false;

    case PROTOCOL_PARSE_STATE_LEN_HI:
        parser->payload_len |= (uint16_t)byte << 8;
        if (parser->payload_len > PROTOCOL_MAX_PAYLOAD)
        {
            protocol_parser_reset(parser);
        }
        else
        {
            parser->state = PROTOCOL_PARSE_STATE_TYPE;
        }
        return false;

    case PROTOCOL_PARSE_STATE_TYPE:
        parser->type = byte;
        parser->payload_index = 0;
        parser->state = (parser->payload_len > 0) ? PROTOCOL_PARSE_STATE_PAYLOAD : PROTOCOL_PARSE_STATE_CRC_LO;
        return false;

    case PROTOCOL_PARSE_STATE_PAYLOAD:
        parser->payload[parser->payload_index++] = byte;
        if (parser->payload_index >= parser->payload_len)
        {
            parser->state = PROTOCOL_PARSE_STATE_CRC_LO;
        }
        return false;

    case PROTOCOL_PARSE_STATE_CRC_LO:
        parser->crc_received = byte;
        parser->state = PROTOCOL_PARSE_STATE_CRC_HI;
        return false;

    case PROTOCOL_PARSE_STATE_CRC_HI:
    default:
    {
        parser->crc_received |= (uint16_t)byte << 8;

        uint8_t crc_input[1 + PROTOCOL_MAX_PAYLOAD];
        crc_input[0] = parser->type;
        if (parser->payload_len > 0)
        {
            memcpy(&crc_input[1], parser->payload, parser->payload_len);
        }
        uint16_t crc_computed = protocol_crc16_ccitt(crc_input, (size_t)parser->payload_len + 1);

        bool valid = (crc_computed == parser->crc_received);
        if (valid && out_frame)
        {
            out_frame->type = parser->type;
            out_frame->payload_len = parser->payload_len;
            if (parser->payload_len > 0)
            {
                memcpy(out_frame->payload, parser->payload, parser->payload_len);
            }
        }

        protocol_parser_reset(parser);
        return valid;
    }
    }
}
