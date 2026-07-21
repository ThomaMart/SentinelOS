#ifndef ROGUE_AP_H
#define ROGUE_AP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#define ROGUE_AP_MAX_ENTRIES 20

/**
 * @brief Lance un scan Wi-Fi bloquant et analyse les résultats à la
 * recherche de signes classiques d'un "evil twin" / rogue AP :
 *   - même SSID diffusé par plusieurs BSSID différents (usurpation
 *     probable d'un réseau existant)
 *   - réseau en clair (OPEN) ou WEP (sécurité faible/obsolète)
 *
 * Bloquant (quelques centaines de ms à ~1s selon le nombre de canaux) --
 * à appeler depuis une tâche dédiée, pas depuis le thread LVGL. Interrompt
 * brièvement la connexion Wi-Fi en cours (comportement normal d'un scan
 * actif ESP-IDF), donc volontairement laissé à la demande plutôt que
 * périodique pour ne pas perturber en continu le radar CSI / RWR.
 */
esp_err_t rogue_ap_scan(void);

/** Nombre total de réseaux vus lors du dernier scan. */
uint8_t rogue_ap_get_count(void);

/** Nombre de réseaux marqués suspects lors du dernier scan. */
uint8_t rogue_ap_get_flagged_count(void);

/** SSID (chaîne C, tronquée à 32 caractères) du réseau d'indice idx. */
const char *rogue_ap_get_ssid(uint8_t idx);

/** Adresse MAC (BSSID) formatée "XX:XX:XX:XX:XX:XX" du réseau d'indice idx. */
const char *rogue_ap_get_bssid_str(uint8_t idx);

/** RSSI (dBm) du réseau d'indice idx. */
int8_t rogue_ap_get_rssi(uint8_t idx);

/** true si le réseau d'indice idx a été marqué suspect. */
bool rogue_ap_is_flagged(uint8_t idx);

/** Raison du marquage ("Duplicate SSID" / "Weak security" / ""). */
const char *rogue_ap_get_flag_reason(uint8_t idx);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_AP_H */
