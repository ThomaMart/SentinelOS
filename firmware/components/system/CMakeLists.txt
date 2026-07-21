idf_component_register(
    SRCS
        "system_info.c"

    INCLUDE_DIRS
        "."

    PRIV_REQUIRES
        esp_timer
        heap
        config
)