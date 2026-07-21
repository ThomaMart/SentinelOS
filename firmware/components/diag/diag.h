#ifndef DIAG_H
#define DIAG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

/**
 * @brief Analyse la raison du dernier reset (esp_reset_reason()) et met à
 * jour les compteurs persistés en NVS -- à appeler tôt au boot, avant
 * tout ce qui pourrait lui-même crasher.
 *
 * Un reset "anormal" (panique, watchdog tâche/interruption, brownout)
 * incrémente un compteur de crashs persistant ; un reset normal (power-on,
 * bouton reset, reset logiciel volontaire) ne l'incrémente pas.
 */
esp_err_t diag_init(void);

/** Chaîne lisible décrivant la raison du dernier reset (ex: "Panic"). */
const char *diag_get_last_reset_reason_str(void);

/** true si le dernier reset est considéré comme anormal (crash). */
bool diag_last_reset_was_crash(void);

/** Nombre total de resets anormaux observés depuis l'origine (persisté). */
uint32_t diag_get_crash_count(void);

/** Nombre total de boots observés depuis l'origine (persisté). */
uint32_t diag_get_boot_count(void);

/**
 * @brief true si un core dump valide est présent dans la partition
 * "coredump" -- capturé automatiquement par ESP-IDF lors du panic/crash
 * précédent (CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH). Analysable après coup
 * avec `idf.py coredump-info`.
 */
bool diag_has_coredump(void);

#ifdef __cplusplus
}
#endif

#endif /* DIAG_H */
