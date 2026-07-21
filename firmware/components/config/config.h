#ifndef CONFIG_H
#define CONFIG_H

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

#define SENTINELOS_UPDATE_MANIFEST_URL "http://192.168.1.52:8080/manifest.json"

#ifdef __cplusplus
}
#endif

#endif