#ifndef STORAGE_H
#define STORAGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

/**
 * @brief Initialise le stockage NVS.
 *
 * À appeler une seule fois au démarrage.
 */
esp_err_t storage_init(void);

/**
 * @brief Sauvegarde une chaîne.
 */
esp_err_t storage_set_string(const char *key,
                             const char *value);

/**
 * @brief Lit une chaîne.
 */
esp_err_t storage_get_string(const char *key,
                             char *buffer,
                             size_t size);

/**
 * @brief Sauvegarde un entier 32 bits.
 */
esp_err_t storage_set_u32(const char *key,
                          uint32_t value);

/**
 * @brief Lit un entier 32 bits.
 */
esp_err_t storage_get_u32(const char *key,
                          uint32_t *value);

/**
 * @brief Efface une clé.
 */
esp_err_t storage_delete(const char *key);

/**
 * @brief Efface tout le namespace SentinelOS.
 */
esp_err_t storage_erase(void);

#ifdef __cplusplus
}
#endif

#endif /* STORAGE_H */