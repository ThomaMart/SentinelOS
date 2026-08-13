#include "config.h"

/* Wi-Fi credentials live in a git-ignored header under firmware/secrets/ so
 * they never get committed. Provision it by copying
 * components/config/wifi_credentials.example.h to
 * firmware/secrets/wifi_credentials.h and filling in your network. */
#include "wifi_credentials.h"

static const char HOSTNAME[] = "SentinelOS";

const char *config_get_wifi_ssid(void)
{
    return SENTINELOS_WIFI_SSID;
}

const char *config_get_wifi_password(void)
{
    return SENTINELOS_WIFI_PASSWORD;
}

const char *config_get_hostname(void)
{
    return HOSTNAME;
}
