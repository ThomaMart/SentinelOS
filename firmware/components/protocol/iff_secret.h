#ifndef IFF_SECRET_H
#define IFF_SECRET_H

#include <stdint.h>

/**
 * @brief Clé symétrique partagée pour l'IFF (Identification Friend or
 * Foe) sur le protocole UART -- HMAC-SHA256 challenge/response.
 *
 * DÉMO uniquement : une vraie mise en production ne coderait jamais un
 * secret symétrique en dur dans le firmware (extractible par quiconque
 * dumpe la flash, même chiffrée -- le firmware en clair la contient).
 * Une implémentation réelle provisionnerait cette clé par device dans
 * un élément sécurisé (ATECC608, eFuse dédié) ou utiliserait un schéma
 * asymétrique comme la signature OTA (ota_pubkey.h) qui n'a ce problème
 * que côté serveur, jamais côté device.
 */
static const uint8_t IFF_SECRET_KEY[32] = {
    0x66, 0x85, 0x0F, 0x96, 0xEE, 0x48, 0xB4, 0xC8,
    0x66, 0xE4, 0x53, 0x06, 0xF8, 0x4F, 0x0A, 0xE4,
    0xA4, 0x02, 0x16, 0xBC, 0xD8, 0x3A, 0xD0, 0x1C,
    0x57, 0x91, 0xB6, 0x54, 0xD8, 0x84, 0xA7, 0x59,
};

#endif /* IFF_SECRET_H */
