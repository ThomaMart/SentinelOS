#ifndef SYSTEM_INFO_H
#define SYSTEM_INFO_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * @brief Initialise le composant System.
 */
void system_init(void);

/**
 * @brief Temps écoulé depuis le démarrage (secondes).
 */
uint32_t system_get_uptime(void);

/**
 * @brief Mémoire libre (heap).
 */
uint32_t system_get_free_heap(void);

/**
 * @brief Plus faible quantité de heap libre observée.
 */
uint32_t system_get_minimum_free_heap(void);

/**
 * @brief Version du firmware.
 */
const char *system_get_version(void);

#ifdef __cplusplus
}
#endif

#endif /* SYSTEM_INFO_H */