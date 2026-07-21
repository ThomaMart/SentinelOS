idf_component_register(
    SRCS
        "protocol.c"
        "protocol_parser.c"

    INCLUDE_DIRS
        "."

    REQUIRES
        esp_driver_uart
        esp_system
        esp_hw_support
        esp_timer
        mbedtls
        config
        ota
        system
)
