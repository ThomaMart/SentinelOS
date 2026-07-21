idf_component_register(
    SRCS
        "rwr.c"

    INCLUDE_DIRS
        "."

    REQUIRES
        esp_wifi
        esp_timer
)
