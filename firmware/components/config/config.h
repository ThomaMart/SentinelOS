#ifndef CONFIG_H
#define CONFIG_H

#include "sdkconfig.h"

#ifdef __cplusplus
extern "C" {
#endif

const char *config_get_wifi_ssid(void);
const char *config_get_wifi_password(void);
const char *config_get_hostname(void);

#define SENTINELOS_NAME        "SentinelOS"
#define SENTINELOS_VERSION     "0.12.0"
#define SENTINELOS_BOARD       "ESP32-2432S028R"
#define SENTINELOS_CPU         "ESP32"

/* OTA manifest URL — configurable via Kconfig (idf.py menuconfig -> SentinelOS,
 * or CONFIG_SENTINELOS_UPDATE_MANIFEST_URL in sdkconfig.defaults). */
#define SENTINELOS_UPDATE_MANIFEST_URL CONFIG_SENTINELOS_UPDATE_MANIFEST_URL

#ifdef __cplusplus
}
#endif

#endif