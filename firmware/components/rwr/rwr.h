#ifndef RWR_H
#define RWR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

/**
 * @brief Démarre le "Radar Warning Receiver" : mode promiscuous Wi-Fi
 * filtré sur les trames de management, pour détecter une rafale de
 * trames deauth/disassoc (signature d'une attaque deauth active dans
 * les parages -- outil classique pour forcer une déconnexion Wi-Fi ou
 * capturer un handshake WPA).
 *
 * Doit être appelé après wifi_init() (le pilote Wi-Fi doit être démarré).
 * Fonctionne indépendamment d'une connexion établie : le mode promiscuous
 * écoute toutes les trames de management sur le canal radio courant.
 */
esp_err_t rwr_init(void);

/**
 * @brief true si une rafale de trames deauth/disassoc a été détectée
 * récemment (fenêtre de quelques secondes).
 */
bool rwr_attack_detected(void);

/**
 * @brief Nombre total de trames deauth/disassoc observées depuis le
 * démarrage.
 */
uint32_t rwr_get_deauth_count(void);

/**
 * @brief Nombre total de trames de management (tous types) observées
 * depuis le démarrage.
 */
uint32_t rwr_get_total_frames(void);

/**
 * @brief Adresse MAC source de la dernière trame deauth/disassoc reçue,
 * formatée "XX:XX:XX:XX:XX:XX". Chaîne vide si aucune trame reçue.
 */
const char *rwr_get_last_attacker(void);

#ifdef __cplusplus
}
#endif

#endif /* RWR_H */
