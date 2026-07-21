#ifndef RADAR_H
#define RADAR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

/**
 * @brief Démarre la capture Wi-Fi CSI (Channel State Information) et
 * l'analyse de présence/mouvement.
 *
 * Doit être appelé après wifi_init() (le pilote Wi-Fi doit être démarré).
 * Aucune association à un AP n'est requise pour recevoir des trames (le
 * CSI est extrait de toute trame reçue par la radio, y compris les
 * beacons des réseaux à proximité), mais la qualité du signal exploité
 * dépend directement de la connexion Wi-Fi existante du reste du firmware.
 */
esp_err_t radar_init(void);

/**
 * @brief Démarre un ping périodique (150 ms) vers la passerelle par
 * défaut, uniquement pour générer un flux de trafic Wi-Fi régulier --
 * sans ça, le CSI n'est capturé que sur le trafic de contrôle
 * occasionnel de la connexion, trop épars pour un suivi en temps réel.
 *
 * À appeler une fois la connexion Wi-Fi établie (IP obtenue).
 */
esp_err_t radar_start_probing(void);

/**
 * @brief Niveau de variation du signal ambiant (0-100), lissé.
 *
 * 0 = signal stable (pas de mouvement détecté récemment).
 * 100 = variation maximale observée depuis le démarrage.
 */
int radar_get_signal_level(void);

/**
 * @brief true si une variation significative du signal a été détectée
 * récemment (fenêtre de quelques centaines de ms).
 */
bool radar_motion_detected(void);

/**
 * @brief Nombre total de trames CSI traitées depuis le démarrage.
 */
uint32_t radar_get_sample_count(void);

#ifdef __cplusplus
}
#endif

#endif /* RADAR_H */
