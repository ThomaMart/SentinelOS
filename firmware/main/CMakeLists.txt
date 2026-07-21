idf_component_register(
    SRCS
        "app.c"

    INCLUDE_DIRS
        "."

    REQUIRES
        display
        wifi
        system
        nvs_flash
        storage
        ota
        config
        protocol
        radar
        rwr
        i2c
        diag
        sdcard
)