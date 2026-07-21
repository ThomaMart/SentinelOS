#ifndef I2C_BUS_H
#define I2C_BUS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "esp_err.h"

#define I2C_BUS_MAX_DEVICES 16

/**
 * @brief Initialise le bus I2C master sur le header d'extension de la
 * carte (SCL=GPIO22, SDA=GPIO27 -- pins standard du connecteur I2C non
 * peuplé sur l'ESP32-2432S028R, libres de tout usage LCD/touch).
 */
esp_err_t i2c_bus_init(void);

/**
 * @brief Sonde toutes les adresses I2C 7 bits valides (0x03-0x77) et met
 * à jour la liste des périphériques détectés. Bloquant, quelques
 * dizaines de ms -- à appeler depuis une tâche dédiée, pas depuis le
 * thread LVGL.
 */
esp_err_t i2c_bus_scan(void);

/** Nombre de périphériques trouvés lors du dernier scan. */
uint8_t i2c_bus_get_count(void);

/** Adresse 7 bits du périphérique d'indice idx (0xFF si hors bornes). */
uint8_t i2c_bus_get_address(uint8_t idx);

#ifdef __cplusplus
}
#endif

#endif /* I2C_BUS_H */
