#ifndef BLE_ALLOWLIST_H
#define BLE_ALLOWLIST_H

#include <stdint.h>

/**
 * @brief Adresses BLE "amies" pour la démo IFF-lite -- ajoute ici
 * l'adresse MAC d'un appareil connu (téléphone, écouteurs...) pour le
 * voir classé FRIEND lors d'un scan. Vide par défaut : tout ce qui est
 * vu par le scan est alors classé "bogey" (inconnu), ce qui est un
 * comportement normal et attendu tant que l'allowlist n'a pas été
 * remplie manuellement.
 *
 * Format : 6 octets, ordre tel qu'affiché par ble_scan_get_addr_str()
 * (ex: téléphone Android "AA:BB:CC:DD:EE:FF" -> {0xAA,0xBB,0xCC,0xDD,0xEE,0xFF}).
 */
static const uint8_t BLE_ALLOWLIST[][6] = {
    /* {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}, */
};

#define BLE_ALLOWLIST_COUNT (sizeof(BLE_ALLOWLIST) / sizeof(BLE_ALLOWLIST[0]))

#endif /* BLE_ALLOWLIST_H */
