#ifndef SDCARD_H
#define SDCARD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#define SDCARD_MOUNT_POINT "/sdcard"

/**
 * @brief Monte une carte micro-SD (FATFS sur SPI) si présente.
 *
 * Partage volontairement le bus SPI déjà utilisé par l'écran LCD
 * (SPI2_HOST, initialisé par bsp_init()) -- l'ESP32 original n'a que
 * deux périphériques SPI généraux (SPI2/SPI3), tous deux déjà pris par
 * l'écran et le tactile respectivement. Ajoute juste un device de plus
 * sur le bus existant (CS dédié, GPIO5), comme documenté par
 * esp_vfs_fat_sdspi_mount() pour ce cas de partage de bus.
 *
 * Doit être appelé après bsp_init() (le bus SPI2 doit déjà exister).
 * Échec propre et non bloquant si aucune carte n'est insérée -- ce n'est
 * pas une erreur fatale pour le reste du firmware.
 *
 * ⚠️ NON VÉRIFIÉ SUR MATÉRIEL : le slot "TF" de ce board n'a aucun label
 * de pin visible sur le PCB (juste des références composants R13/C9/RN2
 * près du connecteur, pas de datasheet/schéma disponible). CS=GPIO5 +
 * partage du bus LCD est le pinout le plus communément documenté pour
 * les boards CYD de ce type, mais deux configurations testées (bus LCD
 * et bus tactile, CS=5 dans les deux cas) échouent identiquement à
 * l'étape SD CMD8 ("send_if_cond"), carte non détectée. Cause probable :
 * mauvais GPIO pour CS (l'erreur identique sur deux bus de données
 * différents pointe vers le CS, pas les lignes MOSI/MISO/CLK) -- mais
 * confirmer nécessiterait un schéma ou une mesure au multimètre.
 */
esp_err_t sdcard_init(void);

/** true si une carte est actuellement montée. */
bool sdcard_is_mounted(void);

/** Capacité totale de la carte en Mo (0 si non montée). */
uint32_t sdcard_get_capacity_mb(void);

/** Espace libre sur la carte en Mo (0 si non montée). */
uint32_t sdcard_get_free_mb(void);

/**
 * @brief Ajoute une ligne horodatée à /sdcard/sentinelos.log.
 *
 * No-op silencieux si la carte n'est pas montée.
 */
void sdcard_append_log(const char *text);

#ifdef __cplusplus
}
#endif

#endif /* SDCARD_H */
