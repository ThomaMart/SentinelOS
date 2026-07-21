#ifndef OTA_H
#define OTA_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

#include "esp_err.h"

/**
 * @brief Initialise le composant OTA.
 */
esp_err_t ota_init(void);

/**
 * @brief Démarre le téléchargement et le flash d'un firmware OTA.
 *
 * Si expected_sha256 est fourni (64 caractères hex), l'image
 * téléchargée est vérifiée par hash avant de basculer le boot
 * dessus ; en cas de mismatch, l'update est abandonnée et le
 * firmware actuel reste actif. Si NULL, aucune vérification
 * d'intégrité n'est faite (déconseillé hors développement).
 *
 * Si expected_signature_hex est fourni (signature ECDSA au format DER,
 * encodée en hex), elle est vérifiée avec la clé publique embarquée
 * (ota_pubkey.h) sur ce même hash ; en cas d'échec, l'update est
 * abandonnée même si le SHA-256 correspond (le hash seul est
 * falsifiable via le manifest, la signature ne l'est pas). Si NULL,
 * seule l'intégrité (hash) est vérifiée, pas l'authenticité.
 *
 * Redémarre l'appareil automatiquement en cas de succès.
 */
esp_err_t ota_start(const char *url, const char *expected_sha256, const char *expected_signature_hex);

/**
 * @brief Indique si une mise à jour est en cours.
 */
bool ota_is_running(void);

/**
 * @brief Retourne la progression (0-100).
 */
int ota_get_progress(void);

/**
 * @brief Récupère le manifest de mise à jour et démarre l'OTA
 * si la version distante diffère de SENTINELOS_VERSION.
 */
esp_err_t ota_check_update(const char *manifest_url);

/**
 * @brief Comme ota_check_update(), mais ne démarre PAS le téléchargement
 * si une mise à jour est trouvée -- se contente de mémoriser ses
 * informations (accessible ensuite via ota_start_pending()) et de
 * mettre à jour le statut affiché.
 */
esp_err_t ota_check_only(const char *manifest_url);

/**
 * @brief Démarre le téléchargement de la mise à jour trouvée par le
 * dernier ota_check_only() ayant détecté une version différente.
 *
 * @return ESP_ERR_INVALID_STATE si aucune mise à jour n'est en attente.
 */
esp_err_t ota_start_pending(void);

/**
 * @brief Confirme que le firmware actuel est valide.
 *
 * À appeler après un test minimal réussi (ex: connexion Wi-Fi).
 * Sans cet appel, une image en attente de validation est
 * automatiquement annulée par le bootloader au reboot suivant
 * (rollback vers la partition précédente).
 */
void ota_confirm_valid(void);

#ifdef __cplusplus
}
#endif

#endif /* OTA_H */