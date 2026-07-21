idf_component_register(
    SRCS
        "display.c"

    INCLUDE_DIRS
        "."

    REQUIRES
        bsp
        wifi
        config
        system
        bootloader_support
        efuse
        radar
        rwr
        i2c
        diag
        rogue_ap
        sdcard
)