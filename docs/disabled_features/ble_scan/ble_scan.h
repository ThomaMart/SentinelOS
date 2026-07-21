#ifndef BLE_SCAN_H
#define BLE_SCAN_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#define BLE_SCAN_MAX_DEVICES 16

/**
 * @brief Initialise la stack BLE (NimBLE, host uniquement -- pas de
 * GATT server, pas d'advertising, juste "observer"/scan). Démarre la
 * tâche hôte NimBLE.
 *
 * Volontairement PAS appelé au boot par app.c : la stack BT consomme
 * assez de RAM/IRAM pour faire échouer d'autres allocations (observé :
 * ping du radar et client HTTP de l'OTA en ESP_ERR_NO_MEM une fois BLE
 * actif en même temps). Init différée à la première utilisation réelle
 * de l'onglet BLE SCAN, une fois le boot critique (OTA, Wi-Fi) terminé.
 * Idempotent -- un second appel est un no-op qui retourne ESP_OK.
 */
esp_err_t ble_scan_init(void);

/**
 * @brief true une fois que le host NimBLE a fini sa synchro avec le
 * contrôleur (callback ble_hs_cfg.sync_cb) -- ble_scan_start() échouera
 * tant que ce n'est pas vrai. Se stabilise en général en quelques
 * centaines de ms après ble_scan_init().
 */
bool ble_scan_is_ready(void);

/**
 * @brief Démarre un scan passif de 5 secondes. Asynchrone -- les
 * résultats se remplissent au fur et à mesure (voir ble_scan_get_count())
 * et ble_scan_is_running() repasse à false une fois terminé.
 *
 * ESP_ERR_INVALID_STATE si un scan est déjà en cours.
 */
esp_err_t ble_scan_start(void);

/** true tant que le scan en cours n'est pas terminé. */
bool ble_scan_is_running(void);

/** Nombre d'appareils BLE distincts vus lors du dernier scan. */
uint8_t ble_scan_get_count(void);

/** Nom annoncé (chaîne C, vide si l'appareil ne diffuse pas de nom). */
const char *ble_scan_get_name(uint8_t idx);

/** Adresse formatée "XX:XX:XX:XX:XX:XX". */
const char *ble_scan_get_addr_str(uint8_t idx);

/** RSSI (dBm). */
int8_t ble_scan_get_rssi(uint8_t idx);

/**
 * @brief true si l'adresse de cet appareil est dans l'allowlist
 * (ble_allowlist.h) -- "FRIEND" au sens IFF-lite. false ("bogey"/inconnu)
 * pour tout le reste, y compris si l'allowlist est vide (par défaut).
 */
bool ble_scan_is_friend(uint8_t idx);

#ifdef __cplusplus
}
#endif

#endif /* BLE_SCAN_H */
