idf_component_register(
    SRCS
        "radar.c"

    INCLUDE_DIRS
        "."

    REQUIRES
        esp_wifi
        esp_timer
        esp_netif
        lwip
)
