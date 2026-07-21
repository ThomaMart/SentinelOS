#ifndef BSP_H
#define BSP_H

#include <esp_err.h>
#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialise toute la carte.
 *
 * Initialise :
 *  - LCD
 *  - LVGL
 *  - Touch
 *  - Backlight
 *
 * @return ESP_OK si tout est correctement initialisé.
 */
esp_err_t bsp_init(void);

/**
 * @brief Retourne le display LVGL.
 *
 * @return Pointeur sur le display LVGL.
 */
lv_display_t *bsp_display_get(void);

/**
 * @brief Active le rétroéclairage.
 *
 * @return ESP_OK si succès.
 */
esp_err_t bsp_backlight_on(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_H */