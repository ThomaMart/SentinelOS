#include <string.h>

#include "protocol.h"
#include "unity.h"

static const uint8_t MAGIC[4] = {0xAA, 0x55, 0xC3, 0x3C};

/* Construit une trame brute dans out_buf, retourne sa taille totale. */
static size_t build_frame(uint8_t type, const uint8_t *payload, uint16_t payload_len, uint8_t *out_buf)
{
    size_t i = 0;
    memcpy(&out_buf[i], MAGIC, sizeof(MAGIC));
    i += sizeof(MAGIC);

    out_buf[i++] = (uint8_t)(payload_len & 0xFF);
    out_buf[i++] = (uint8_t)(payload_len >> 8);
    out_buf[i++] = type;

    if (payload_len > 0)
    {
        memcpy(&out_buf[i], payload, payload_len);
        i += payload_len;
    }

    uint8_t crc_input[1 + PROTOCOL_MAX_PAYLOAD];
    crc_input[0] = type;
    if (payload_len > 0)
    {
        memcpy(&crc_input[1], payload, payload_len);
    }
    uint16_t crc = protocol_crc16_ccitt(crc_input, (size_t)payload_len + 1);

    out_buf[i++] = (uint8_t)(crc & 0xFF);
    out_buf[i++] = (uint8_t)(crc >> 8);

    return i;
}

/* Nourrit tous les octets sauf le dernier (aucune trame ne doit encore sortir),
 * puis retourne le résultat du dernier octet. */
static bool feed_all(protocol_parser_t *parser, const uint8_t *buf, size_t len, protocol_frame_t *out_frame)
{
    bool result = false;
    for (size_t i = 0; i < len; i++)
    {
        result = protocol_parser_feed(parser, buf[i], out_frame);
        if (i < len - 1)
        {
            TEST_ASSERT_FALSE_MESSAGE(result, "frame completed before all bytes were fed");
        }
    }
    return result;
}

TEST_CASE("CRC16/CCITT-FALSE matches the official check value", "[protocol]")
{
    /* Vecteur de test officiel du CRC-16/CCITT-FALSE pour l'ASCII "123456789". */
    const uint8_t input[] = "123456789";
    uint16_t crc = protocol_crc16_ccitt(input, sizeof(input) - 1);
    TEST_ASSERT_EQUAL_HEX16(0x29B1, crc);
}

TEST_CASE("Parses a valid frame with no payload", "[protocol]")
{
    protocol_parser_t parser;
    protocol_parser_reset(&parser);

    uint8_t buf[16];
    size_t len = build_frame(0x01, NULL, 0, buf);

    protocol_frame_t frame = {0};
    bool got = feed_all(&parser, buf, len, &frame);

    TEST_ASSERT_TRUE(got);
    TEST_ASSERT_EQUAL_UINT8(0x01, frame.type);
    TEST_ASSERT_EQUAL_UINT16(0, frame.payload_len);
}

TEST_CASE("Parses a valid frame with a payload", "[protocol]")
{
    protocol_parser_t parser;
    protocol_parser_reset(&parser);

    const uint8_t payload[] = {0xDE, 0xAD, 0xBE, 0xEF, 0x00};
    uint8_t buf[32];
    size_t len = build_frame(0x02, payload, sizeof(payload), buf);

    protocol_frame_t frame = {0};
    bool got = feed_all(&parser, buf, len, &frame);

    TEST_ASSERT_TRUE(got);
    TEST_ASSERT_EQUAL_UINT8(0x02, frame.type);
    TEST_ASSERT_EQUAL_UINT16(sizeof(payload), frame.payload_len);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(payload, frame.payload, sizeof(payload));
}

TEST_CASE("Ignores garbage/log noise and resyncs on the next valid frame", "[protocol]")
{
    protocol_parser_t parser;
    protocol_parser_reset(&parser);

    /* Bruit incluant un octet identique au premier octet du magic (0xAA),
     * pour vérifier que la resynchronisation partielle ne casse rien. */
    const uint8_t noise[] = {'l', 'o', 'g', ':', ' ', 0xAA, 'x', 'y', 0x55, 'z'};
    protocol_frame_t frame = {0};
    for (size_t i = 0; i < sizeof(noise); i++)
    {
        bool got = protocol_parser_feed(&parser, noise[i], &frame);
        TEST_ASSERT_FALSE_MESSAGE(got, "noise must never be parsed as a valid frame");
    }

    uint8_t buf[16];
    size_t len = build_frame(0x01, NULL, 0, buf);
    bool got = feed_all(&parser, buf, len, &frame);

    TEST_ASSERT_TRUE(got);
    TEST_ASSERT_EQUAL_UINT8(0x01, frame.type);
}

TEST_CASE("Rejects an oversized length and recovers on the next frame", "[protocol]")
{
    protocol_parser_t parser;
    protocol_parser_reset(&parser);

    uint8_t bogus[7];
    memcpy(bogus, MAGIC, sizeof(MAGIC));
    uint16_t bogus_len = PROTOCOL_MAX_PAYLOAD + 1;
    bogus[4] = (uint8_t)(bogus_len & 0xFF);
    bogus[5] = (uint8_t)(bogus_len >> 8);
    bogus[6] = 0x02; /* type, jamais atteint car rejeté avant */

    protocol_frame_t frame = {0};
    for (size_t i = 0; i < sizeof(bogus); i++)
    {
        bool got = protocol_parser_feed(&parser, bogus[i], &frame);
        TEST_ASSERT_FALSE(got);
    }

    uint8_t buf[16];
    size_t len = build_frame(0x01, NULL, 0, buf);
    bool got = feed_all(&parser, buf, len, &frame);

    TEST_ASSERT_TRUE_MESSAGE(got, "parser must recover after an oversized-length frame");
    TEST_ASSERT_EQUAL_UINT8(0x01, frame.type);
}

TEST_CASE("Drops a frame with a bad CRC and recovers on the next frame", "[protocol]")
{
    protocol_parser_t parser;
    protocol_parser_reset(&parser);

    uint8_t buf[16];
    size_t len = build_frame(0x01, NULL, 0, buf);
    buf[len - 1] ^= 0xFF; /* corrompt le CRC */

    protocol_frame_t frame = {0};
    bool got = feed_all(&parser, buf, len, &frame);
    TEST_ASSERT_FALSE_MESSAGE(got, "frame with corrupted CRC must be dropped");

    uint8_t buf2[16];
    size_t len2 = build_frame(0x02, NULL, 0, buf2);
    got = feed_all(&parser, buf2, len2, &frame);

    TEST_ASSERT_TRUE_MESSAGE(got, "parser must recover after a bad-CRC frame");
    TEST_ASSERT_EQUAL_UINT8(0x02, frame.type);
}
