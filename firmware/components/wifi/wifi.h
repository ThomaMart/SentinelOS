#ifndef WIFI_H
#define WIFI_H

#include <stdbool.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the Wi-Fi subsystem.
 *
 * Initializes:
 *  - TCP/IP stack
 *  - Event loop
 *  - Wi-Fi driver
 *  - Station interface
 *
 * The Wi-Fi driver is started but no connection is made.
 *
 * @return ESP_OK on success.
 */
esp_err_t wifi_init(void);

/**
 * @brief Connect to the configured Wi-Fi network.
 *
 * Connection logic will be implemented in the next sprint.
 *
 * @return ESP_OK on success.
 */
esp_err_t wifi_connect(void);

/**
 * @brief Disconnect from the current Wi-Fi network.
 */
void wifi_disconnect(void);

/**
 * @brief Returns the current Wi-Fi connection state.
 *
 * @return true if connected.
 * @return false otherwise.
 */
bool wifi_is_connected(void);

#ifdef __cplusplus
}
#endif

#endif /* WIFI_H */