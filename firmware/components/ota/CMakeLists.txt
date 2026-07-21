idf_component_register(
    SRCS
        "ota.c"

    INCLUDE_DIRS
        "."

    REQUIRES
        esp_https_ota
        esp_http_client
        app_update
        esp_system
        esp-tls
        cjson
        config
        display
        mbedtls
)