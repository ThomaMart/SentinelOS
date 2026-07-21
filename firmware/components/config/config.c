#include "config.h"

static const char WIFI_SSID[] = "***REDACTED-WIFI-SSID***";
static const char WIFI_PASSWORD[] = "***REDACTED-WIFI-PASSWORD***";
static const char HOSTNAME[] = "SentinelOS";

const char *config_get_wifi_ssid(void)
{
    return WIFI_SSID;
}

const char *config_get_wifi_password(void)
{
    return WIFI_PASSWORD;
}

const char *config_get_hostname(void)
{
    return HOSTNAME;
}